#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/mutex.h>

#include <linux/uaccess.h> // copy_from/to_user
#include <asm/uaccess.h> // ^same
#include <linux/sched.h> // for timers
#include <linux/jiffies.h> // for jiffies global variable
#include <linux/string.h> // for string manipulation functions
#include <linux/ctype.h> // for isdigit
#include <linux/font.h> // for default font

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Meteor game");

#define CYG_FB_DEFAULT_PALETTE_BLUE         0x01
#define CYG_FB_DEFAULT_PALETTE_RED          0x04
#define CYG_FB_DEFAULT_PALETTE_WHITE        0x0F
#define CYG_FB_DEFAULT_PALETTE_LIGHTBLUE    0x09
#define CYG_FB_DEFAULT_PALETTE_BLACK        0x00

// Device file definitions
static int meteor_open(struct inode *inode, struct file *filp);
static int meteor_release(struct inode *inode, struct file *filp);
static ssize_t meteor_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static ssize_t meteor_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);

struct file_operations meteor_fops = {
write:
    meteor_write,
read:
    meteor_read,
open:
    meteor_open,
release:
    meteor_release,
};

// Global variables for meteors and character
struct fb_info *info;
struct fb_fillrect *blank;
const struct font_desc *current_font;

// Helper functions
/* Helper function borrowed from drivers/video/fbdev/core/fbmem.c */
static struct fb_info *get_fb_info(unsigned int idx)
{
    struct fb_info *fb_info;

    if (idx >= FB_MAX)
        return ERR_PTR(-ENODEV);

    fb_info = registered_fb[idx];
    if (fb_info)
        atomic_inc(&fb_info->count);

    return fb_info;
}

// Device file functions
static int __init meteor_init(void)
{
    // Draw a rectagle
    blank = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
    blank->dx = 500;
    blank->dy = 0;
    blank->width = 40;
    blank->height = 100;
    blank->color = CYG_FB_DEFAULT_PALETTE_RED;
    blank->rop = ROP_COPY;
    info = get_fb_info(0);
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);

    return 0;
}

static void __exit meteor_exit(void) {
    kfree(blank);
    if (info) {
        atomic_dec(&info->count);
    }

    printk(KERN_INFO "Module exiting\n");
}

static int meteor_open(struct inode *inode, struct file *filp) {
}

static int meteor_release(struct inode *inode, struct file *filp) {
}

static ssize_t meteor_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
}

static ssize_t meteor_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
}

module_init(meteor_init);
module_exit(meteor_exit);

