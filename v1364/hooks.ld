/* Executable VAs, NOT firmware addresses; only the hash-pinned V13.6.3 SELF. */
original_orphan_scan = 0x22e00;
original_native_unlock = 0x20920;
original_root1 = 0x3f40d0;
original_root_resume = 0x22e2b;
original_load_api = 0x234e0;
original_decrypt = 0x23600;
ps_open = 0x34de00;
ps_close = 0x34de20;
ps_mkdir = 0x34de40;
ps_write = 0x34de50;
ps_fsync = 0x34de60;
ps_load = 0x34df10;
ps_dlsym = 0x34df20;
tu_errno = 0x24f6a;
tu_ioctl = 0x253ca;
mount_init_ptr = 0x40e168;
mount_ptr = 0x40e170;
unmount_init_ptr = 0x40e178;
unmount_ptr = 0x40e180;
SECTIONS {
  . = 0x3f0900;
  .payload : { *(.text .text.*) *(.rodata .rodata.*) }
  .forbidden : { *(.data .data.*) *(.bss .bss.*) *(.got*) *(.plt*) }
  ASSERT(SIZEOF(.forbidden) == 0, "Hook requires unsupported writable state/GOT")
  ASSERT(. <= 0x3f4000, "Hook exceeds verified code cave")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
