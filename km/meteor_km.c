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
static void meteor_handler(struct timer_list*);

struct file_operations meteor_fops = {
write:
    meteor_write,
open:
    meteor_open,
release:
    meteor_release,
};

// Global variables for meteors and character
struct fb_info *info;
struct fb_fillrect *blank;

typedef struct meteor_position {
    int dx;
    int dy;
    int width;
    int height;
} meteor_position_t;

static struct timer_list * timer;
static int meteor_update_rate_ms = 10;
static meteor_position_t *meteors[32];
static int n_meteors = 0;
static int meteor_falling_rate = 5;

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

static int redraw_meteor(meteor_position_t *old_position, meteor_position_t *new_position) {
    // Draw rectangle at the old position in black
    blank->dx = old_position->dx;
    blank->dy = old_position->dy;
    blank->width = old_position->width;
    blank->height = old_position->height;
    blank->color = CYG_FB_DEFAULT_PALETTE_BLACK;
    blank->rop = ROP_COPY;
    info = get_fb_info(0);
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);

    // Draw rectangle at new position in red
    blank->dx = new_position->dx;
    blank->dy = new_position->dy;
    blank->width = new_position->width;
    blank->height = new_position->height;
    blank->color = CYG_FB_DEFAULT_PALETTE_BLACK;
    blank->rop = ROP_COPY;
    info = get_fb_info(0);
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);
}

// meteor timer handler
static void meteor_handler(struct timer_list *data) {
    // Move all meteors down a few pixels
    // int i;
    // for (i=0; i<n_meteors; i++) {
        // meteor_position_t *new_meteor_position = meteors[i];
        // new_meteor_position->dy = meteors[i] + meteor_falling_rate;
        // redraw_meteor(meteors[i], new_meteor_position);
    // }

    // Restart timer
    printk(KERN_ALERT "Timer up\n");
    mod_timer(timer, jiffies + msecs_to_jiffies(meteor_update_rate_ms));
}

// Device file functions
static int __init meteor_init(void)
{
    // Make memory for drawing rectangles
    blank = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
    if (!blank) {
        pr_err("Failed to allocate new blank pointer");
    }

    // Start the meteor timer
    timer_setup(timer, meteor_handler, 0);
    mod_timer(timer, jiffies + msecs_to_jiffies(meteor_update_rate_ms));

    // TEST Draw a meteor and have it fall
    meteor_position_t *new_position = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
    printk(KERN_ALERT "drawing new meteor\n");
    if (!new_position) {
        pr_err("Failed to allocate new meteor pointer");
    }
    new_position->dx = 200;
    new_position->dy = 0;
    new_position->width = 40;
    new_position->height = 40;

    blank->dx = new_position->dx;
    blank->dy = new_position->dy;
    blank->width = new_position->width;
    blank->height = new_position->height;
    blank->color = CYG_FB_DEFAULT_PALETTE_RED;
    blank->rop = ROP_COPY;
    info = get_fb_info(0);
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);

    if (n_meteors < 32) {
        printk(KERN_ALERT "Adding new meteor to list\n");
        meteors[n_meteors] = new_position;
        n_meteors ++;
    } else {
        pr_warn("Meteor array full, freeing allocated instance");
        kfree(new_position);
    }

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

static ssize_t meteor_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {

}

static int meteor_release(struct inode *inode, struct file *filp) {
}
module_init(meteor_init);
module_exit(meteor_exit);

