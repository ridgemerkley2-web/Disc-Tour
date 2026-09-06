using System;
using System.Diagnostics;

internal static class DGTourAcceptanceLauncher
{
    [STAThread]
    private static void Main()
    {
        const string executable = @"C:\DGTour_Packages\S19_WindowsShipping_20260825T055948Z_b0635a4c7f19\Windows\DiscGolfTour\Binaries\Win64\DiscGolfTour-Win64-Shipping.exe";
        const string workingDirectory = @"C:\DGTour_Packages\S19_WindowsShipping_20260825T055948Z_b0635a4c7f19\Windows\DiscGolfTour\Binaries\Win64";
        const string arguments = @"-UserDir=""C:\DGTour_Acceptance\S19_UI_20260825T060700Z_b0635a4c7f19"" -windowed -ResX=1280 -ResY=720";

        Process.Start(new ProcessStartInfo
        {
            FileName = executable,
            Arguments = arguments,
            WorkingDirectory = workingDirectory,
            UseShellExecute = false
        });
    }
}
