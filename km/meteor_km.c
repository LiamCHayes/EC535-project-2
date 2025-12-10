#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>

#include <linux/uaccess.h> // copy_from/to_user
#include <asm/uaccess.h> // ^same
#include <linux/sched.h> // for timers
#include <linux/jiffies.h> // for jiffies global variable
#include <linux/string.h> // for string manipulation functions
#include <linux/ctype.h> // for isdigit
#include <linux/font.h> // for default font
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Meteor game");

#define CYG_FB_DEFAULT_PALETTE_BLUE         0x01
#define CYG_FB_DEFAULT_PALETTE_RED          0x04
#define CYG_FB_DEFAULT_PALETTE_WHITE        0x0F
#define CYG_FB_DEFAULT_PALETTE_LIGHTBLUE    0x09
#define CYG_FB_DEFAULT_PALETTE_BLACK        0x00
#define CYG_FB_DEFAULT_PALETTE_GREEN        0x02
#define CYG_FB_DEFAULT_PALETTE_PINK         0x0D
#define CYG_FB_DEFAULT_PALETTE_YELLOW       0x0E
#define CYG_FB_DEFAULT_PALETTE_LIGHTGREEN   0x0A

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

// Meteor updates
static struct timer_list * timer;
static int meteor_update_rate_ms = 100;
static meteor_position_t *meteors[32];
static meteor_position_t * new_meteor_position;
static int n_meteors = 0;
static int meteor_falling_rate = 4;
static int meteor_size = 75;
static struct mutex meteor_mutex;

// Handle meteor color changes
static int meteor_colors[7] = {
    CYG_FB_DEFAULT_PALETTE_BLUE,
    CYG_FB_DEFAULT_PALETTE_WHITE,
    CYG_FB_DEFAULT_PALETTE_RED,
    CYG_FB_DEFAULT_PALETTE_GREEN,
    CYG_FB_DEFAULT_PALETTE_PINK,
    CYG_FB_DEFAULT_PALETTE_YELLOW,
    CYG_FB_DEFAULT_PALETTE_LIGHTGREEN};
static int n_meteor_colors = 7;
static int meteor_color_idx = 0;
static int meteor_color;

// Temporary varibles for updating meteor and character positions
static meteor_position_t * character;
static meteor_position_t * new_character_position;

// Game over global variables
static const uint8_t font_5x7[][7] = {
    // G
    {
        0b01110,
        0b10001,
        0b10000,
        0b10111,
        0b10001,
        0b10001,
        0b01110
    },
    // A
    {
        0b01110,
        0b10001,
        0b10001,
        0b11111,
        0b10001,
        0b10001,
        0b10001
    },
    // M
    {
        0b10001,
        0b11011,
        0b10101,
        0b10101,
        0b10001,
        0b10001,
        0b10001
    },
    // E
    {
        0b11111,
        0b10000,
        0b10000,
        0b11110,
        0b10000,
        0b10000,
        0b11111
    },
    // O
    {
        0b01110,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b01110
    },
    // V
    {
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b01010,
        0b00100
    },
    // R
    {
        0b11110,
        0b10001,
        0b10001,
        0b11110,
        0b10100,
        0b10010,
        0b10001
    },
};
static enum { G, A, M, E, O, V, R };

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
    sys_fillrect(info, blank);

    // Draw rectangle at new position in red
    blank->dx = new_position->dx;
    blank->dy = new_position->dy;
    blank->width = new_position->width;
    blank->height = new_position->height;
    blank->color = CYG_FB_DEFAULT_PALETTE_LIGHTBLUE;
    blank->rop = ROP_COPY;
    sys_fillrect(info, blank);
}

static int redraw_meteor(meteor_position_t *old_position, meteor_position_t *new_position) {
    // Draw rectangle at the old position in black
    blank->dx = old_position->dx;
    blank->dy = old_position->dy;
    blank->width = old_position->width;
    blank->height = old_position->height;
    blank->color = CYG_FB_DEFAULT_PALETTE_BLACK;
    blank->rop = ROP_COPY;
    sys_fillrect(info, blank);

    // Draw rectangle at new position in red
    blank->dx = new_position->dx;
    blank->dy = new_position->dy;
    blank->width = new_position->width;
    blank->height = new_position->height;
    blank->color = meteor_color;
    blank->rop = ROP_COPY;
    sys_fillrect(info, blank);
}

static void draw_rect(struct fb_info *info, int x, int y, int w, int h, u32 color) {
    struct fb_fillrect rect = {
        .dx = x,
        .dy = y,
        .width = w,
        .height = h,
        .color = color,
        .rop = ROP_COPY,
    };
    sys_fillrect(info, &rect);
}

static void draw_char(struct fb_info *info, int letter_index,
                      int x, int y, int pixel_size, u32 color)
{
    for (int row = 0; row < 7; row++) {
        uint8_t line = font_5x7[letter_index][row];

        for (int col = 0; col < 5; col++) {
            if (line & (1 << (4 - col))) {
                draw_rect(info,
                          x + col * pixel_size,
                          y + row * pixel_size,
                          pixel_size,
                          pixel_size,
                          color);
            }
        }
    }
}

void draw_game_over(struct fb_info *info, int start_x, int start_y,
                    int pixel_size, u32 color)
{
    int x = start_x;

    draw_char(info, G, x, start_y, pixel_size, color); x += 6 * pixel_size;
    draw_char(info, A, x, start_y, pixel_size, color); x += 6 * pixel_size;
    draw_char(info, M, x, start_y, pixel_size, color); x += 6 * pixel_size;
    draw_char(info, E, x, start_y, pixel_size, color); x += 8 * pixel_size; // gap

    draw_char(info, O, x, start_y, pixel_size, color); x += 6 * pixel_size;
    draw_char(info, V, x, start_y, pixel_size, color); x += 6 * pixel_size;
    draw_char(info, E, x, start_y, pixel_size, color); x += 6 * pixel_size;
    draw_char(info, R, x, start_y, pixel_size, color);
}

// meteor timer handler
static void meteor_handler(struct timer_list *data) {

    // Move all meteors down a few pixels
    mutex_lock(&meteor_mutex);
    int i;
    for (i=0; i<n_meteors; ) {
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
            printk(KERN_ALERT "Deleting meteor %d\n", i);
            printk(KERN_ALERT "Number of meteors before deleting %d\n", n_meteors);
            kfree(meteors[i]);
            int j;
            for (j=i; j<n_meteors-1; j++) {
                meteors[j] = meteors[j+1];
            }
            n_meteors--;
            printk(KERN_ALERT "Number of meteors after deleting %d\n", n_meteors);
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

static int meteor_release(struct inode *inode, struct file *filp) {
    printk(KERN_ALERT "Releasing the file!\n");
    del_timer_sync(timer);
    kfree(character);
    int i;
    for (i = 0; i < n_meteors; i++) {
        if (meteors[i]) {
            kfree(meteors[i]);
            meteors[i] = NULL; // Best practice
        }
    }
    n_meteors = 0;
    meteor_color_idx = 0;
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

    // Bounds checking for security
    if (character_x > 500 || spawn_x > 500) {
        return count;
    }

    mutex_lock(&meteor_mutex);
    if (character_x < 0 && spawn_x < 280) {
        // Increase meteor spawn rate
        meteor_falling_rate = spawn_x;

        // Update meteor color
        meteor_color_idx++;
        if (meteor_color_idx >= n_meteor_colors) {
            meteor_color_idx = 0;
        }
        meteor_color = meteor_colors[meteor_color_idx];
    } else {
        // Redraw the character
        new_character_position->dx = character_x;
        new_character_position->dy = 250;
        new_character_position->width = 20;
        new_character_position->height = 20;
        redraw_character(character, new_character_position);
        character->dx = character_x;

        // Check if there is a collision
        int i;
        int meteor_x;
        int meteor_y;
        for (i=0; i<n_meteors; i++) {
            meteor_x = meteors[i]->dx;
            meteor_y = meteors[i]->dy;
            int x_difference = character_x - meteor_x;
            if (meteor_y > 280 - (meteor_size + 31)) {
                if (x_difference > -20 && x_difference < meteor_size) {
                    printk(KERN_ALERT "Collision detected\n");

                    // Redraw screen to black
                    meteor_position_t *new_position = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
                    if (!new_position) {
                        pr_err("Failed to allocate new meteor pointer");
                        mutex_unlock(&meteor_mutex);
                        return -ENOMEM;
                    }
                    new_position->dx = 0;
                    new_position->dy = 0;
                    new_position->width = 500;
                    new_position->height = 280;

                    blank->dx = new_position->dx;
                    blank->dy = new_position->dy;
                    blank->width = new_position->width;
                    blank->height = new_position->height;
                    blank->color = CYG_FB_DEFAULT_PALETTE_BLACK;
                    blank->rop = ROP_COPY;
                    sys_fillrect(info, blank);

                    draw_game_over(info, 100, 50, 10, CYG_FB_DEFAULT_PALETTE_WHITE);
                    mutex_unlock(&meteor_mutex);
                    return -2;
                }
            }
        }

        // Add a new meteor
        if (spawn_x > 0) {
            if (n_meteors < 32) {
                // Check if a meteor is colliding with another meteor
                for (i=0; i<n_meteors; i++) {
                    meteor_x = meteors[i]->dx;
                    meteor_y = meteors[i]->dy;
                    int x_difference = spawn_x - meteor_x;
                    if (meteor_y < meteor_size) {
                        if (x_difference > -meteor_size && x_difference < meteor_size) {
                            printk(KERN_ALERT "Meteor spawned at x=%d is in collision with another meteor", spawn_x);
                            mutex_unlock(&meteor_mutex);
                            return count;
                        }
                    }
                }

                printk(KERN_ALERT "drawing new meteor at %d\n", spawn_x);
                printk(KERN_ALERT "Number of meteors before adding %d\n", n_meteors);
                meteor_position_t *new_position = kmalloc(sizeof(meteor_position_t), GFP_KERNEL);
                if (!new_position) {
                    pr_err("Failed to allocate new meteor pointer");
                    mutex_unlock(&meteor_mutex);
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
                blank->color = meteor_color;
                blank->rop = ROP_COPY;
                sys_fillrect(info, blank);

                meteors[n_meteors] = new_position;
                n_meteors ++;

                printk(KERN_ALERT "Number of meteors after adding %d\n", n_meteors);
            } else {
                printk(KERN_ALERT "reached max number of meteors, skipping this creation\n");
            }
        }
    }

    mutex_unlock(&meteor_mutex);
    return count;
}

module_init(meteor_init);
module_exit(meteor_exit);

