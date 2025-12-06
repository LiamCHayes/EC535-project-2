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

void draw_text(struct fb_info *info, int x, int y, const char *text, u32 color) {
    struct fb_image image;
    int i;

    if (!current_font || !info)
        return;

    image.width = current_font->width;
    image.height = current_font->height;
    image.fg_color = color;
    image.bg_color = CYG_FB_DEFAULT_PALETTE_BLACK; // Background color index
    image.depth = info->var.bits_per_pixel;
    image.cmap.len = 0; // Not using a custom colormap for the image

    for (i = 0; text[i] != '\0'; i++) {
        unsigned char c = text[i];
        // The font data is a monochrome bitmap
        image.data = current_font->data + (c * current_font->height * ((current_font->width + 7) / 8));
        image.dx = x + i * current_font->width;
        image.dy = y;

        // Use the system's image blit function to draw the character
        sys_imageblit(info, &image);
    }
}

// Device file functions
static int __init meteor_init(void)
{
    // Get framebuffer info and lock
    info = get_fb_info(0);
    if (IS_ERR(info)) {
        printk(KERN_ERR "meteor_init: Failed to get fb_info\n");
        return PTR_ERR(info);
    }
    lock_fb_info(info);

    // get system default font to write text to screen
    current_font = get_default_font(0, 0, ~0, ~0); 
    if (!current_font) {
        printk(KERN_ERR "meteor_init: Failed to get default font\n");
        unlock_fb_info(info);
        atomic_dec(&info->count);
        return -ENODEV;
    }

    // Draw a rectagle
    blank = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
    blank->dx = 400;
    blank->dy = 0;
    blank->width = 40;
    blank->height = 100;
    blank->color = CYG_FB_DEFAULT_PALETTE_RED;
    blank->rop = ROP_COPY;
    info = get_fb_info(0);
    sys_fillrect(info, blank);

    // Write some words to the screen using the new function
    draw_text(info, 10, 10, "READY TO START THE METOR GAME????", CYG_FB_DEFAULT_PALETTE_WHITE);

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

