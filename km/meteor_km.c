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
static void meteor_handler(struct timer_list*);

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

typedef struct meteor_position {
    int dx;
    int dy;
    int width;
    int height;
} meteor_position_t;

static struct timer_list * timer;
static int meteor_update_rate_ms = 100;
static meteor_position_t *meteors[32];
static int meteors_x[32];
static int meteors_y[32];
static struct mutex meteor_mutex;
static meteor_position_t * new_meteor_position;
static int n_meteors = 0;
static int meteor_falling_rate = 4;
static int meteor_size = 100;

static meteor_position_t * character;
static meteor_position_t * new_character_position;

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

static int redraw_character(meteor_position_t *old_position, meteor_position_t *new_position) {
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
    blank->color = CYG_FB_DEFAULT_PALETTE_LIGHTBLUE;
    blank->rop = ROP_COPY;
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);
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
    mutex_lock(&meteor_mutex);
    for (i=0; i<n_meteors; ) {
        // Redraw meteor
        new_meteor_position->dx = meteors[i]->dx;
        new_meteor_position->dy = meteors[i]->dy + meteor_falling_rate;
        new_meteor_position->width = meteors[i]->width;
        new_meteor_position->height = meteors[i]->height;
        redraw_meteor(meteors[i], new_meteor_position);

        // Update meteor position in list
        meteors[i]->dy = meteors[i]->dy + meteor_falling_rate;
        meteors_y[i] = meteors[i]->dy + meteor_falling_rate;

        // Delete meteor if it went past the screen
        if (meteors[i]->dy > 280) {
            kfree(meteors[i]);
            int j;
            for (j=i; j<n_meteors-1; j++) {
                meteors[j] = meteors[j+1];
                meteors_x[j] = meteors_x[j+1];
                meteors_y[j] = meteors_y[j+1];
            }
            n_meteors--;
        } else {
            i++;
        }
    }
    mutex_unlock(&meteor_mutex);

    // Restart timer
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

    // Allocate memory for a temporary meteor and character to update positions
    new_meteor_position = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
    if (!new_meteor_position) {
        pr_err("Failed to allocate new meteor pointer");
        kfree(blank);
        kfree(timer);
        return -ENOMEM;
    }

    new_character_position = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
    if (!new_character_position) {
        pr_err("Failed to allocate new character pointer");
        kfree(blank);
        kfree(timer);
        kfree(new_meteor_position);
        return -ENOMEM;
    }

    // Meteor array mutex
    mutex_init(&meteor_mutex);

    // Initialize framebuffer info
    info = get_fb_info(0);

    printk(KERN_INFO "Module initialized!\n");

    return 0;
}

static void __exit meteor_exit(void) {
    del_timer_sync(timer);

    int i;
    for (i = 0; i < n_meteors; i++) {
        if (meteors[i]) {
            kfree(meteors[i]);
            meteors[i] = NULL; // Best practice
        }
    }
    n_meteors = 0;

    kfree(blank);
    kfree(timer);
    kfree(new_meteor_position);
    kfree(new_character_position);
    if (character) {
        kfree(character);
    }
    if (info) {
        atomic_dec(&info->count);
    }

    unregister_chrdev(61, "meteor_dash");

    printk(KERN_INFO "Module exiting\n");
}

static int meteor_open(struct inode *inode, struct file *filp) {
    printk(KERN_ALERT "Opening the file!\n");

    // start the timer
    timer_setup(timer, meteor_handler, 0);
    mod_timer(timer, jiffies + msecs_to_jiffies(meteor_update_rate_ms));
    printk(KERN_ALERT "Started the timer!\n");

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
    lock_fb_info(info);
    sys_fillrect(info, blank);
    unlock_fb_info(info);

    printk(KERN_ALERT "Added the character!");

    return 0;
}

static ssize_t meteor_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
}

static ssize_t meteor_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
    // Read from userspace
    char buffer[8];
    int ret;
    ret = copy_from_user(&buffer, buf, count);
    if (ret != 0) {
        pr_err("failed to copy bytes from userspace\n");
        return -EFAULT;
    }

    // Parse message
    char *temp_str;
    char *character_location;
    char *spawn_location;
    int character_x;
    int spawn_x;
    const char *delimiter = ",";
    temp_str = buffer;
    character_location = strsep(&temp_str, delimiter);
    spawn_location = strsep(&temp_str, delimiter);

    // Cast to int
    ret = kstrtoint(character_location, 10, &character_x);
    if (ret < 0) {
        pr_err("Failed to parse character to int\n");
    }
    ret = kstrtoint(spawn_location, 10, &spawn_x);
    if (ret < 0) {
        pr_err("Failed to parse spawn to int\n");
    }

    if (character_x < 0) {
        meteor_falling_rate = spawn_x;
    } else {
        // Redraw the character
        printk(KERN_ALERT "drawing character at %d\n", character_x);
        new_character_position->dx = character_x;
        new_character_position->dy = 250;
        new_character_position->width = 20;
        new_character_position->height = 20;
        redraw_character(character, new_character_position);
        character->dx = character_x;

        // Check if there is a collision
        // int i;
        // int meteor_x;
        // int meteor_y;
        // for (i=0; i<n_meteors; ) {
            // meteor_x = meteors_x[i];
            // meteor_y = meteors_y[i];
            // int x_difference = character_x - meteor_x;
            // if (meteor_y > meteor_size + 20) {
                // if (x_difference > 0 && x_difference < meteor_size) {
                    // printk(KERN_INFO "Collision detected\n", character_x);
                    // return -2;
                // }
            // }
        // }

        // Add a new meteor
        if (spawn_x > 0) {
            if (n_meteors < 32) {
                printk(KERN_ALERT "drawing new meteor at %d\n", spawn_x);
                meteor_position_t *new_position = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
                if (!new_position) {
                    pr_err("Failed to allocate new meteor pointer");
                    return -ENOMEM;
                }
                new_position->dx = spawn_x;
                new_position->dy = 0;
                new_position->width = meteor_size;
                new_position->height = meteor_size;

                blank->dx = new_position->dx;
                blank->dy = new_position->dy;
                blank->width = new_position->width;
                blank->height = new_position->height;
                blank->color = CYG_FB_DEFAULT_PALETTE_RED;
                blank->rop = ROP_COPY;
                lock_fb_info(info);
                sys_fillrect(info, blank);
                unlock_fb_info(info);

                mutex_lock(&meteor_mutex);
                printk(KERN_ALERT "Adding new meteor to list\n");
                meteors[n_meteors] = new_position;
                n_meteors ++;
                mutex_unlock(&meteor_mutex);

                meteors_x[n_meteors] = spawn_x;
                meteors_y[n_meteors] = 0;
            } else {
                printk(KERN_ALERT "reached max number of meteors, skipping this creation\n");
            }
        }
    }

    return count;
}

static int meteor_release(struct inode *inode, struct file *filp) {
}

module_init(meteor_init);
module_exit(meteor_exit);

