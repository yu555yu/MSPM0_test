using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

internal static class BluetoothSppTool
{
    private const int BluetoothMaxNameSize = 248;
    private const uint ErrorSuccess = 0;
    private const uint ErrorNoMoreItems = 259;
    private const uint BluetoothServiceEnable = 1;

    private static readonly Guid SerialPortService =
        new Guid("00001101-0000-1000-8000-00805F9B34FB");

    [StructLayout(LayoutKind.Sequential)]
    private struct SystemTime
    {
        public ushort Year;
        public ushort Month;
        public ushort DayOfWeek;
        public ushort Day;
        public ushort Hour;
        public ushort Minute;
        public ushort Second;
        public ushort Milliseconds;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct BluetoothDeviceInfo
    {
        public uint Size;
        public ulong Address;
        public uint ClassOfDevice;

        [MarshalAs(UnmanagedType.Bool)]
        public bool Connected;

        [MarshalAs(UnmanagedType.Bool)]
        public bool Remembered;

        [MarshalAs(UnmanagedType.Bool)]
        public bool Authenticated;

        public SystemTime LastSeen;
        public SystemTime LastUsed;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = BluetoothMaxNameSize)]
        public string Name;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BluetoothDeviceSearchParams
    {
        public uint Size;

        [MarshalAs(UnmanagedType.Bool)]
        public bool ReturnAuthenticated;

        [MarshalAs(UnmanagedType.Bool)]
        public bool ReturnRemembered;

        [MarshalAs(UnmanagedType.Bool)]
        public bool ReturnUnknown;

        [MarshalAs(UnmanagedType.Bool)]
        public bool ReturnConnected;

        [MarshalAs(UnmanagedType.Bool)]
        public bool IssueInquiry;

        public byte TimeoutMultiplier;
        public IntPtr Radio;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BluetoothFindRadioParams
    {
        public uint Size;
    }

    [DllImport("bthprops.cpl", SetLastError = true)]
    private static extern IntPtr BluetoothFindFirstDevice(
        ref BluetoothDeviceSearchParams searchParams,
        ref BluetoothDeviceInfo deviceInfo);

    [DllImport("bthprops.cpl", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool BluetoothFindNextDevice(
        IntPtr findHandle,
        ref BluetoothDeviceInfo deviceInfo);

    [DllImport("bthprops.cpl")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool BluetoothFindDeviceClose(IntPtr findHandle);

    [DllImport("bthprops.cpl", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern uint BluetoothAuthenticateDevice(
        IntPtr parentWindow,
        IntPtr radio,
        ref BluetoothDeviceInfo deviceInfo,
        string passkey,
        uint passkeyLength);

    [DllImport("bthprops.cpl", SetLastError = true)]
    private static extern uint BluetoothSetServiceState(
        IntPtr radio,
        ref BluetoothDeviceInfo deviceInfo,
        ref Guid serviceGuid,
        uint serviceFlags);

    [DllImport("bthprops.cpl", SetLastError = true)]
    private static extern IntPtr BluetoothFindFirstRadio(
        ref BluetoothFindRadioParams findParams,
        out IntPtr radio);

    [DllImport("bthprops.cpl")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool BluetoothFindRadioClose(IntPtr findHandle);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CloseHandle(IntPtr handle);

    private static BluetoothDeviceInfo NewDeviceInfo()
    {
        BluetoothDeviceInfo info = new BluetoothDeviceInfo();
        info.Size = (uint)Marshal.SizeOf(typeof(BluetoothDeviceInfo));
        return info;
    }

    private static List<BluetoothDeviceInfo> Scan(byte timeoutMultiplier)
    {
        BluetoothDeviceSearchParams search = new BluetoothDeviceSearchParams();
        search.Size = (uint)Marshal.SizeOf(typeof(BluetoothDeviceSearchParams));
        search.ReturnAuthenticated = true;
        search.ReturnRemembered = true;
        search.ReturnUnknown = true;
        search.ReturnConnected = true;
        search.IssueInquiry = true;
        search.TimeoutMultiplier = timeoutMultiplier;
        search.Radio = IntPtr.Zero;

        List<BluetoothDeviceInfo> devices = new List<BluetoothDeviceInfo>();
        BluetoothDeviceInfo info = NewDeviceInfo();
        IntPtr findHandle = BluetoothFindFirstDevice(ref search, ref info);
        if (findHandle == IntPtr.Zero)
        {
            int error = Marshal.GetLastWin32Error();
            if (error == 0 || error == (int)ErrorNoMoreItems)
            {
                return devices;
            }
            throw new Win32Exception(error, "Bluetooth inquiry failed");
        }

        try
        {
            do
            {
                devices.Add(info);
                info = NewDeviceInfo();
            }
            while (BluetoothFindNextDevice(findHandle, ref info));
        }
        finally
        {
            BluetoothFindDeviceClose(findHandle);
        }

        return devices;
    }

    private static string FormatAddress(ulong address)
    {
        string text = address.ToString("X12");
        StringBuilder result = new StringBuilder(17);
        for (int index = 0; index < 12; index += 2)
        {
            if (result.Length > 0)
            {
                result.Append(':');
            }
            result.Append(text.Substring(index, 2));
        }
        return result.ToString();
    }

    private static void PrintDevice(BluetoothDeviceInfo device)
    {
        Console.WriteLine(
            "{0,-24} {1} paired={2} remembered={3} connected={4}",
            string.IsNullOrEmpty(device.Name) ? "(unknown)" : device.Name,
            FormatAddress(device.Address),
            device.Authenticated,
            device.Remembered,
            device.Connected);
    }

    private static IntPtr OpenFirstRadio(out IntPtr radio)
    {
        BluetoothFindRadioParams parameters = new BluetoothFindRadioParams();
        parameters.Size = (uint)Marshal.SizeOf(typeof(BluetoothFindRadioParams));
        IntPtr findHandle = BluetoothFindFirstRadio(ref parameters, out radio);
        if (findHandle == IntPtr.Zero || radio == IntPtr.Zero)
        {
            throw new Win32Exception(Marshal.GetLastWin32Error(), "No Bluetooth radio found");
        }
        return findHandle;
    }

    private static int Pair(string targetName, string pin, byte timeoutMultiplier)
    {
        List<BluetoothDeviceInfo> devices = Scan(timeoutMultiplier);
        BluetoothDeviceInfo? target = null;
        foreach (BluetoothDeviceInfo device in devices)
        {
            if (string.Equals(device.Name, targetName, StringComparison.OrdinalIgnoreCase))
            {
                target = device;
                break;
            }
        }

        if (!target.HasValue)
        {
            Console.Error.WriteLine("没有扫描到经典蓝牙设备：{0}", targetName);
            return 2;
        }

        BluetoothDeviceInfo selected = target.Value;
        PrintDevice(selected);

        IntPtr radio = IntPtr.Zero;
        IntPtr radioFindHandle = IntPtr.Zero;
        try
        {
            radioFindHandle = OpenFirstRadio(out radio);

            if (!selected.Authenticated)
            {
                uint authResult = BluetoothAuthenticateDevice(
                    IntPtr.Zero,
                    radio,
                    ref selected,
                    pin,
                    (uint)pin.Length);
                if (authResult != ErrorSuccess && authResult != ErrorNoMoreItems)
                {
                    Console.Error.WriteLine(
                        "配对失败：Win32 error {0} ({1})",
                        authResult,
                        new Win32Exception((int)authResult).Message);
                    return 3;
                }
                Console.WriteLine("配对成功。等待启用 SPP 服务……");
            }
            else
            {
                Console.WriteLine("设备已经配对。检查 SPP 服务……");
            }

            Guid spp = SerialPortService;
            uint serviceResult = BluetoothSetServiceState(
                radio,
                ref selected,
                ref spp,
                BluetoothServiceEnable);
            if (serviceResult == ErrorSuccess)
            {
                Console.WriteLine("SPP 服务已启用，Windows 将创建传出 COM 口。");
            }
            else
            {
                Console.WriteLine(
                    "SPP 启用返回 {0} ({1})；若服务已启用可忽略，随后检查 COM 口。",
                    serviceResult,
                    new Win32Exception((int)serviceResult).Message);
            }
        }
        finally
        {
            if (radioFindHandle != IntPtr.Zero)
            {
                BluetoothFindRadioClose(radioFindHandle);
            }
            if (radio != IntPtr.Zero)
            {
                CloseHandle(radio);
            }
        }

        return 0;
    }

    private static void PrintUsage()
    {
        Console.WriteLine("BluetoothSppTool scan [timeoutMultiplier]");
        Console.WriteLine("BluetoothSppTool pair [deviceName] [pin] [timeoutMultiplier]");
        Console.WriteLine("timeoutMultiplier 单位 1.28 秒，默认 8（约 10 秒）。");
    }

    public static int Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        Console.InputEncoding = Encoding.UTF8;

        if (args.Length == 0)
        {
            PrintUsage();
            return 1;
        }

        try
        {
            string command = args[0].ToLowerInvariant();
            if (command == "scan")
            {
                byte timeout = args.Length >= 2 ? byte.Parse(args[1]) : (byte)8;
                List<BluetoothDeviceInfo> devices = Scan(timeout);
                Console.WriteLine("扫描到 {0} 个经典蓝牙设备：", devices.Count);
                foreach (BluetoothDeviceInfo device in devices)
                {
                    PrintDevice(device);
                }
                return 0;
            }

            if (command == "pair")
            {
                string name = args.Length >= 2 ? args[1] : "HC-04";
                string pin = args.Length >= 3 ? args[2] : "1234";
                byte timeout = args.Length >= 4 ? byte.Parse(args[3]) : (byte)8;
                return Pair(name, pin, timeout);
            }

            PrintUsage();
            return 1;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error.Message);
            return 10;
        }
    }
}
