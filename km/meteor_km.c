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
static int meteor_update_rate_ms = 100;
static meteor_position_t *meteors[32];
static meteor_position_t * new_meteor_position;
static int n_meteors = 0;
static int meteor_falling_rate = 2;

static meteor_position_t * character;

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
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);

    // Draw rectangle at new position in red
    blank->dx = new_position->dx;
    blank->dy = new_position->dy;
    blank->width = new_position->width;
    blank->height = new_position->height;
    blank->color = CYG_FB_DEFAULT_PALETTE_RED;
    blank->rop = ROP_COPY;
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);
}

// meteor timer handler
static void meteor_handler(struct timer_list *data) {
    // Move all meteors down a few pixels
    int i;
    for (i=0; i<n_meteors; ) {
        printk(KERN_ALERT "Redrawing meteor %d at %d\n", i, meteors[i]->dy + meteor_falling_rate);
        // Redraw meteor
        new_meteor_position->dx = meteors[i]->dx;
        new_meteor_position->dy = meteors[i]->dy + meteor_falling_rate;
        new_meteor_position->width = meteors[i]->width;
        new_meteor_position->height = meteors[i]->height;
        redraw_meteor(meteors[i], new_meteor_position);

        // Update meteor position in list
        meteors[i]->dy = meteors[i]->dy + meteor_falling_rate;

        // Delete meteor if it went past the screen
        if (meteors[i]->dy > 280) {
            kfree(meteors[i]);
            int j;
            for (j=i; j<n_meteors-1; j++) {
                meteors[j] = meteors[j+1];
            }
            n_meteors--;
        } else {
            i++;
        }
    }

    // Restart timer
    printk(KERN_ALERT "timer up");
    mod_timer(timer, jiffies + msecs_to_jiffies(meteor_update_rate_ms));
}

// Device file functions
static int __init meteor_init(void)
{
    // Device file
    int registration;
    registration = register_chrdev(61, "meteor_dash", &meteor_fops);
    if (registration < 0) { 
        pr_err("could not register device file");
        return registration;
    }

    // Make memory for drawing rectangles
    blank = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
    if (!blank) {
        printk(KERN_ALERT "could not allocate space for blank\n");
        pr_err("Failed to allocate new blank pointer");
        return -ENOMEM;
    }

    // Allocate memory for the meteor timer
    timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
    if (!timer)
    {
        printk(KERN_ALERT "Insufficient kernel memory\n");
        pr_err("Failed to allocate new timer pointer");
        kfree(blank);
        return -ENOMEM;
    }

    // Allocate memory for a temporary meteor to update positions
    new_meteor_position = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
    if (!new_meteor_position) {
        pr_err("Failed to allocate new meteor pointer");
        kfree(blank);
        kfree(timer);
        return -ENOMEM;
    }

    return 0;
}

static void __exit meteor_exit(void) {
    del_timer_sync(timer);
    kfree(blank);
    kfree(timer);
    kfree(new_meteor_position);
    if (character) {
        kfree(character);
    }
    if (info) {
        atomic_dec(&info->count);
    }

    printk(KERN_INFO "Module exiting\n");
}

static int meteor_open(struct inode *inode, struct file *filp) {
    // TODO check if we are adding a new meteor
    // start the timer
    timer_setup(timer, meteor_handler, 0);
    mod_timer(timer, jiffies + msecs_to_jiffies(meteor_update_rate_ms));

    // add the character
    character = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
    printk(KERN_ALERT "drawing the character\n");
    if (!character) {
        pr_err("Failed to allocate new character pointer");
        return -ENOMEM;
    }
    character->dx = 250;
    character->dy = 250;
    character->width = 20;
    character->height = 20;

    blank->dx = character->dx;
    blank->dy = character->dy;
    blank->width = character->width;
    blank->height = character->height;
    blank->color = CYG_FB_DEFAULT_PALETTE_LIGHTBLUE;
    blank->rop = ROP_COPY;
    info = get_fb_info(0);
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);

    // TODO move the character
}

static ssize_t meteor_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
    size_t bytes_to_copy = 16;
    char buffer[16];
    int ret;
    ret = copy_from_user(&buffer, buf, bytes_to_copy);
    if (ret != 0) {
        pr_err("failed to copy bytes from userspace\n");
        return -EFAULT;
    }
    printk(KERN_ALERT, "%s\n", buffer);


    // Add character
    meteor_position_t *new_position = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
    printk(KERN_ALERT "drawing new meteor\n");
    if (!new_position) {
        pr_err("Failed to allocate new meteor pointer");
        return -ENOMEM;
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
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);

    if (n_meteors < 32) {
        printk(KERN_ALERT "Adding new meteor to list\n");
        meteors[n_meteors] = new_position;
        n_meteors ++;
    }

}

static int meteor_release(struct inode *inode, struct file *filp) {
}

module_init(meteor_init);
module_exit(meteor_exit);

