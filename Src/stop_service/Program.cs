using System.ServiceProcess;
using System.Threading;

namespace stop_service
{
    class Program
    {
        static void Main(string[] args)
        {
            using (var sc = new ServiceController("ClipboardFirewall"))
                sc.Stop();
            while (true)
            {
                using (var sc = new ServiceController("ClipboardFirewall"))
                    if (sc.Status == ServiceControllerStatus.Stopped)
                        break;
                Thread.Sleep(100);
            }
        }
    }
}
