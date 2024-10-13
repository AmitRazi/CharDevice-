
# CharDevice - Encrypted Character Device Driver

This project implements a simple Linux kernel module that creates a character device (`/dev/chardevice`) with basic read/write functionality and XOR-based encryption for written data. It also exposes the device size through the `sysfs`.

## Features
- **Character device**: Provides `/dev/chardevice` for reading and writing data.
- **Encryption**: Data written to the device is XOR-encrypted using a configurable encryption key.
- **Sysfs Support**: Exposes the size of the data buffer via `/sys/kernel/chardevice/size`.
- **IOCTL Control**: Allows the encryption key to be set dynamically using an IOCTL command.
- **Thread-safe**: Uses mutex locks to synchronize access to the device.

## Usage

### Build and Load the Module
```bash
make
sudo insmod chardevice.ko encrpytion_key=1234
```

### Create Device File
```bash
sudo mknod /dev/chardevice c <MAJOR_NUM> 0
```
Replace `<MAJOR_NUM>` with the major number displayed when the module is loaded.

### Write Data to Device
```bash
echo "Hello, world!" > /dev/chardevice
```

### Read Data from Device
```bash
cat /dev/chardevice
```

### Set Encryption Key Using IOCTL
```c
#define IOCTL_SET_KEY _IOW(MAJOR_NUM, 0, uint32_t)
```
Example usage in userspace (C code):
```c
uint32_t new_key = 5678;
ioctl(fd, IOCTL_SET_KEY, &new_key);
```

### Remove the Module
```bash
sudo rmmod chardevice
```

## Sysfs
Check the size of the data in the buffer:
```bash
cat /sys/kernel/chardevice/size
```

## License
This project is licensed under the GPL.

---

### Notes:
- Make sure to update the `<MAJOR_NUM>` with the correct major number assigned by the system when loading the module.
- Provide a proper `Makefile` to compile the kernel module (`chardevice.ko`).
