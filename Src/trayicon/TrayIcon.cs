using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using trayicon.Properties;

namespace trayicon
{
    class TrayIcon : IDisposable
    {
        private NotifyIcon _notifyIcon;
        private ContextMenu _contextMenu;
        private MenuItem _menuItem;
        private IContainer _components;
        private bool _running = true;
        private Thread _thread;

        const string ServiceName = "ClipboardShield";

        public TrayIcon()
        {
            CreateNotifyicon();
            _thread = new Thread(ThreadFunction);
            _thread.Start();
        }

        private void CreateNotifyicon()
        {
            _components = new Container();
            _contextMenu = new ContextMenu();
            _menuItem = new MenuItem
            {
                Index = 0,
                Text = "E&xit",
                Tag = this,
                Visible = true
            };
            _menuItem.Click += menuItem1_Click;

            _contextMenu.MenuItems.Add(_menuItem);
            _notifyIcon = new NotifyIcon(_components)
            {
                Icon = Resources.UnkIcon,
                ContextMenu = _contextMenu,
                Text = "Clipboard Shield",
                Visible = true
            };
        }
        
        private static void menuItem1_Click(object Sender, EventArgs e)
        {
            ((TrayIcon) ((MenuItem) Sender).Tag).OnExit();
        }

        private void OnExit()
        {
            _notifyIcon.Visible = false;
            Application.Exit();
        }

        public void Dispose()
        {
            if (_thread != null)
            {
                _running = false;
                _thread.Join();
                _thread = null;
            }
            if (_notifyIcon != null)
            {
                _notifyIcon.Dispose();
                _notifyIcon = null;
            }
            if (_notifyIcon != null)
            {
                _contextMenu.Dispose();
                _contextMenu = null;
            }
            if (_notifyIcon != null)
            {
                _menuItem.Dispose();
                _menuItem = null;
            }
            if (_notifyIcon != null)
            {
                _components.Dispose();
                _components = null;
            }
        }

        private void ThreadFunction()
        {
            while (_running)
            {
                var state = ServiceIsRunning();
                if (!state.HasValue)
                    _notifyIcon.Icon = Resources.UnkIcon;
                else
                    _notifyIcon.Icon = state.Value ? Resources.OnIcon : Resources.OffIcon;
                Thread.Sleep(1000);
            }
        }

        private bool? ServiceIsRunning()
        {
            try
            {
                using (var sc = new ServiceController(ServiceName))
                {
                    switch (sc.Status)
                    {
                        case ServiceControllerStatus.Paused:
                        case ServiceControllerStatus.PausePending:
                        case ServiceControllerStatus.Running:
                            return true;
                        case ServiceControllerStatus.Stopped:
                            return false;
                        case ServiceControllerStatus.StartPending:
                        case ServiceControllerStatus.ContinuePending:
                        case ServiceControllerStatus.StopPending:
                            return null;
                        default:
                            throw new ArgumentOutOfRangeException();
                    }
                }
            }
            catch
            {
            }
            return null;
        }
    }
}
