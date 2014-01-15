/*
 * This file is part of the DSLogic project.
 */

#include "libsigrok.h"
#include "hardware/DSLogic/dslogic.h"

#ifdef __linux__
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif/*__linux__*/

#ifdef _WIN32
#include <windows.h>

#include <initguid.h>
#include <ddk/usbiodef.h>
#include <Setupapi.h>

#include <tchar.h>
#include <conio.h>
#include <dbt.h>
#include <stdio.h>
#include <winuser.h>

#endif/*_WIN32*/



#ifdef __linux__
static void dev_list_add(struct libusbhp_t *h, const char *path,
                         unsigned short vid, unsigned short pid)
{
  struct dev_list_t *dev =
    (struct dev_list_t*)malloc(sizeof(struct dev_list_t));
  dev->path = strdup(path);
  dev->vid = vid;
  dev->pid = pid;
  dev->next = NULL;

  struct dev_list_t *p = h->devlist;
  if(!p) {
    h->devlist = dev;
    return;
  }

  while(p->next) {
    p = p->next;
  }

  p->next = dev;
}

static int dev_list_remove(struct libusbhp_t *h, const char *path)
{
  struct dev_list_t *p = h->devlist;
  if(!p) return 1;

  if(!strcmp(p->path, path)) {
    h->devlist = p->next;
    free(p->path);
    free(p);
    return 0;
  }

  while(p->next) {
    if(!strcmp(p->next->path, path)) {
      struct dev_list_t *pp = p->next;
      p->next = pp->next;
      free(pp->path);
      free(pp->next);
      free(pp);
      return 0;
    }
    p = p->next;
  }

  // Not found
  return 1;
}

static int dev_list_find(struct libusbhp_t *h, const char *path,
                         unsigned short *vid, unsigned short *pid)
{
  struct dev_list_t *p = h->devlist;
  while(p) {
    if(!strcmp(p->path, path)) {
      *vid = p->vid;
      *pid = p->pid;
      return 0;
    }
    p = p->next;
  }

  // Not found
  return 1;
}
#endif/*__linux__*/

#ifdef _WIN32
SR_PRIV LRESULT CALLBACK WinProcCallback(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  struct libusbhp_t *h = (struct libusbhp_t*)GetWindowLong(hwnd, GWL_USERDATA);

  switch(msg) {
  case WM_DEVICECHANGE:
    {
      PDEV_BROADCAST_HDR phdr = (PDEV_BROADCAST_HDR)lp;

      if(!phdr || phdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) break;

      PDEV_BROADCAST_DEVICEINTERFACE devif =
        (PDEV_BROADCAST_DEVICEINTERFACE)lp;

      HDEVINFO devinfolist = SetupDiCreateDeviceInfoList(NULL, NULL);
      
      SP_DEVICE_INTERFACE_DATA devifdata;
      memset(&devifdata, 0, sizeof(devifdata));
      devifdata.cbSize = sizeof(devifdata);
      BOOL b = SetupDiOpenDeviceInterface(devinfolist, devif->dbcc_name, 0,
                                          &devifdata);
      
      DWORD required;
      SP_DEVICE_INTERFACE_DETAIL_DATA devdetaildata;
      memset(&devdetaildata, 0, sizeof(devdetaildata));
      devdetaildata.cbSize = sizeof(devdetaildata);

      SP_DEVINFO_DATA devinfodata;
      memset(&devinfodata, 0, sizeof(devinfodata));
      devinfodata.cbSize = sizeof(devinfodata);
      b = SetupDiGetDeviceInterfaceDetail(devinfolist, &devifdata,
                                          &devdetaildata,
                                          sizeof(devdetaildata),
                                          &required, &devinfodata);
     
      TCHAR deviceidw[1024];
      b = SetupDiGetDeviceInstanceIdW(devinfolist, &devinfodata, deviceidw,
                                      sizeof(deviceidw), NULL);

      char deviceid[1024];
      //size_t sz;
      //wcstombs_s(&sz, deviceid, deviceidw, sizeof(deviceid) - 1);
      wcstombs(deviceid, deviceidw, sizeof(deviceid) - 1);

      char *vid = strstr(deviceid, "VID_");
      if(vid != NULL) vid += 4;

      char *pid = strstr(deviceid, "PID_");
      if(pid != NULL)  pid += 4;

      struct libusbhp_device_t *device = NULL;

      if(pid || vid) {
        device =
          (struct libusbhp_device_t*)malloc(sizeof(struct libusbhp_device_t));
      }

      if(pid) {
        pid[4] = '\0';
        device->idProduct = (unsigned short)strtol(pid, NULL, 16); 
      }

      if(vid) {
        vid[4] = '\0';
        device->idVendor = (unsigned short)strtol(vid, NULL, 16);
      }

      if ((device->idVendor == supported_fx2[0].vid) &&
          (device->idProduct == supported_fx2[0].pid)) {
          switch(wp) {
          case DBT_DEVICEARRIVAL:
            if(h->attach) h->attach(device, h->user_data);
            break;
          case DBT_DEVICEREMOVECOMPLETE:
            if(h->detach) h->detach(device, h->user_data);
            break;
          case DBT_DEVNODES_CHANGED:
          default:
            break;
          }
      }

      if(device) free(device);
    }
    break;
  default:
    break;
  }
  
  return DefWindowProc(hwnd, msg, wp, lp);
}
#endif/*OS_WINDOWS*/

SR_API int libusbhp_init(struct libusbhp_t **handle)
{
  struct libusbhp_t *h = (struct libusbhp_t *)malloc(sizeof(struct libusbhp_t));

  h->attach = NULL;
  h->detach = NULL;
  h->user_data = NULL;

#ifdef __linux__
  h->devlist = NULL;

  // create the udev object
  h->hotplug = udev_new();
  if(!h->hotplug)
  {
    printf("Cannot create udev object\n");
    free(h);
    return 1;
  }

  // create the udev monitor
  h->hotplug_monitor = udev_monitor_new_from_netlink(h->hotplug, "udev");

  // start receiving hotplug events
  udev_monitor_filter_add_match_subsystem_devtype(h->hotplug_monitor,
                                                  "usb", "usb_device");
  udev_monitor_enable_receiving(h->hotplug_monitor);

  struct udev_enumerate *de = udev_enumerate_new (h->hotplug);
  udev_enumerate_add_match_subsystem(de, "usb");
  udev_enumerate_scan_devices(de);

  struct udev_list_entry *lst = udev_enumerate_get_list_entry(de);
  while(lst) {
    struct udev_device *dev =
      udev_device_new_from_syspath(h->hotplug,
                                   udev_list_entry_get_name(lst));

    if(udev_device_get_devnode(dev)) {
      unsigned short idVendor =
        strtol(udev_device_get_sysattr_value(dev, "idVendor"), NULL, 16); 
      unsigned short idProduct =
        strtol(udev_device_get_sysattr_value(dev, "idProduct"), NULL, 16); 

      dev_list_add(h, udev_device_get_devnode(dev), idVendor, idProduct);
    }

    udev_device_unref(dev);

    lst = udev_list_entry_get_next(lst);
  }

  udev_enumerate_unref(de);

#endif/*__linux__*/

#ifdef _WIN32
  memset(&h->wcex, 0, sizeof(h->wcex));
  h->wcex.cbSize = sizeof(WNDCLASSEX);
  h->wcex.lpfnWndProc = WinProcCallback;
  h->wcex.hInstance = GetModuleHandle(NULL);  
  h->wcex.lpszClassName = TEXT("UsbHotplugClass");
  h->wcex.cbWndExtra = sizeof(struct libusbhp_t*); // Size of data.

  RegisterClassEx(&h->wcex);

  h->hwnd =
    CreateWindowEx(0, h->wcex.lpszClassName, TEXT("UsbHotplug"), 0, 0, 0, 0,
                   0, 0, NULL, GetModuleHandle(NULL), NULL);

  SetWindowLong(h->hwnd, GWL_USERDATA, (LONG)h);


  DEV_BROADCAST_DEVICEINTERFACE *filter =
  (DEV_BROADCAST_DEVICEINTERFACE*)malloc(sizeof(DEV_BROADCAST_DEVICEINTERFACE));

  memset(filter, 0, sizeof(DEV_BROADCAST_DEVICEINTERFACE));
  filter->dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
  filter->dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  filter->dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

  h->hDeviceNotify =
    RegisterDeviceNotification(h->hwnd, filter, DEVICE_NOTIFY_WINDOW_HANDLE);
  
  if(h->hDeviceNotify == 0) {
    //printf("RegisterDeviceNotification error\n");
    free(h);
    return 1;
  }
#endif/*_WIN32*/

  *handle = h;
  return 0;
}

SR_API void libusbhp_exit(struct libusbhp_t *h)
{
#ifdef __linux__
  // destroy the udev monitor
  udev_monitor_unref(h->hotplug_monitor);

  // destroy the udev object
  udev_unref(h->hotplug);
#endif/*__linux__*/

#ifdef _WIN32
  UnregisterDeviceNotification(h->hDeviceNotify);
  DestroyWindow(h->hwnd);
  UnregisterClass(h->wcex.lpszClassName, h->wcex.hInstance);
#endif/*_WIN32*/

  free(h);
}

SR_API int libusbhp_handle_events_timeout(struct libusbhp_t *h, struct timeval *tv)
{
  int ms = tv->tv_sec * 1000 + tv->tv_usec / 1000;

#ifdef __linux__
  // create the poll item
  struct pollfd items[1];
  items[0].fd = udev_monitor_get_fd(h->hotplug_monitor);
  items[0].events = POLLIN;
  items[0].revents = 0;

  // while there are hotplug events to process
  while(poll(items, 1, ms) > 0) {    
    // receive the relevant device
    struct udev_device* dev = udev_monitor_receive_device(h->hotplug_monitor);
    if(!dev) {
      // error receiving device, skip it
      continue;
    }

    if(!strcmp(udev_device_get_action(dev), "add")) {
      struct libusbhp_device_t device;

      device.idVendor =
        strtol(udev_device_get_sysattr_value(dev, "idVendor"), NULL, 16); 
      device.idProduct =
        strtol(udev_device_get_sysattr_value(dev, "idProduct"), NULL, 16); 

      dev_list_add(h, udev_device_get_devnode(dev),
                   device.idVendor, device.idProduct);

      if(h->attach) h->attach(&device, h->user_data);
    }

    if(!strcmp(udev_device_get_action(dev), "remove")) {
      struct libusbhp_device_t device;

      int res = dev_list_find(h, udev_device_get_devnode(dev),
                              &device.idVendor, &device.idProduct);

      if(res) {
        if(h->detach) h->detach(NULL, h->user_data);
      } else {
	dev_list_remove(h, udev_device_get_devnode(dev));
        if(h->detach) h->detach(&device, h->user_data);
      }
    }

    // destroy the relevant device
    udev_device_unref(dev);

    // clear the revents
    items[0].revents = 0;
  }
#endif/*__linux__*/

#ifdef _WIN32
  UINT_PTR timer = SetTimer(h->hwnd, 0, ms, NULL);

  MSG msg; 
  int ret = GetMessage(&msg, NULL, 0, 0);

  if(ret <= 0) return 0;
  
  TranslateMessage(&msg);
  DispatchMessage(&msg);

  KillTimer(h->hwnd, timer);
#endif/*_WIN32*/

  return 0;
}

SR_API void libusbhp_register_hotplug_listeners(struct libusbhp_t *handle,
                                         libusbhp_hotplug_cb_fn connected_cb,
                                         libusbhp_hotplug_cb_fn disconnected_cb,
                                         void *user_data)
{
  handle->attach = connected_cb;
  handle->detach = disconnected_cb;
  handle->user_data = user_data;
}
