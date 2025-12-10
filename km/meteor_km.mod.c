#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xa16cf51b, "module_layout" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x1af2db6e, "registered_fb" },
	{ 0xe0c33a74, "__mutex_do_init" },
	{ 0x65e3c22d, "__rt_mutex_init" },
	{ 0x1f6e82a4, "__register_chrdev" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0x1e047854, "warn_slowpath_fmt" },
	{ 0x5f754e5a, "memset" },
	{ 0x373db350, "kstrtoint" },
	{ 0x85df9b6c, "strsep" },
	{ 0x28cc25db, "arm_copy_from_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x6fb021c9, "lock_fb_info" },
	{ 0xcf8d2e6f, "kmem_cache_alloc_trace" },
	{ 0xb44e414c, "kmalloc_caches" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0x526c3a6c, "jiffies" },
	{ 0xd09cfa79, "_mutex_unlock" },
	{ 0x336aa937, "sys_fillrect" },
	{ 0xdd18bb88, "_mutex_lock" },
	{ 0x37a0cba, "kfree" },
	{ 0x97934ecf, "del_timer_sync" },
	{ 0x7c32d0f0, "printk" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

