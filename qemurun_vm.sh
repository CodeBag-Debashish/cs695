#qemurun_vm.sh
sudo qemu-system-x86_64 \
-s \
-S \
-smp 2 \
-m 1024M \
-hda /var/lib/libvirt/images/ub01.qcow2 \
-enable-kvm \
-kernel arch/x86/boot/bzImage \
-append "root=/dev/sda1 console=tty0 nokaslr"
