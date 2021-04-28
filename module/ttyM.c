#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <crypto/hash.h>


#define TINY_TTY_MAJOR      255 /* experimental range */
#define TINY_TTY_MINORS     1   

struct tiny_serial {
    struct tty_struct   *tty;       /* pointer to the tty for this device */
    int         open_count;         /* number of times this port has been opened */
};

static struct tiny_serial *tiny_serial; /* initially all NULL */

static int tiny_open(struct tty_struct *tty, struct file *file)
{
    struct tty_port *port;
     pr_info("%s\n", __func__);
    tty->driver_data = NULL;
    port = tty->port;
    /* get the serial object associated with this tty pointer */
    if(tiny_serial == NULL) {
        /* first time accessing this device, let's create it */
        tiny_serial = kmalloc(sizeof(*tiny_serial), GFP_KERNEL);
        if (!tiny_serial)
            return -ENOMEM;
        tiny_serial->open_count = 0;
    }

    /* save our structure within the tty structure */
    tty->driver_data = tiny_serial;
    tiny_serial->tty = tty;

    ++tiny_serial->open_count;

    return 0;
}

static void tiny_close(struct tty_struct *tty, struct file *file)
{
    struct tiny_serial *tiny = tty->driver_data;
    printk(KERN_INFO "tty diver data open count is %d", tiny_serial->open_count);
    printk(KERN_INFO "tiny data open count is %d", tiny_serial->open_count);
    pr_info("%s\n", __func__);

    if (tiny)
        --tiny->open_count;
}   

struct sdesc {
    struct shash_desc shash;
    char ctx[];
};

static struct sdesc *init_sdesc(struct crypto_shash *alg)
{
    struct sdesc *sdesc;
    int size;

    size = sizeof(struct shash_desc) + crypto_shash_descsize(alg);
    sdesc = kmalloc(size, GFP_KERNEL);
    if (!sdesc)
        return ERR_PTR(-ENOMEM);
    sdesc->shash.tfm = alg;
    return sdesc;
}

static int calc_hash(struct crypto_shash *alg,
             const unsigned char *data, unsigned int datalen,
             unsigned char *digest)
{
    struct sdesc *sdesc;
    int ret;

    sdesc = init_sdesc(alg);
    if (IS_ERR(sdesc)) {
        pr_info("can't alloc sdesc\n");
        return PTR_ERR(sdesc);
    }

    ret = crypto_shash_digest(&sdesc->shash, data, datalen, digest);
    kfree(sdesc);
    return ret;
}


static int tiny_write(struct tty_struct *tty, 
              const unsigned char *buffer, int count)
{
    unsigned char *out_digest;
    struct tty_port *port;
    struct tiny_serial *tiny = tty->driver_data;
    int i,j,k;
    int retval = -EINVAL;
    char * tmp_buf = NULL;
    struct crypto_shash *alg;
    char buf[64];
    char *hash_alg_name = "sha256";
    unsigned int datalen;
    size_t l;

    tty->driver_data = tiny_serial;
    printk(KERN_INFO "on write initially");
    if (!tiny)
        return -ENODEV; 
    printk(KERN_INFO "tty driver data open count is %d", tiny->open_count); 
    port = tty->port;
    tiny->tty = tty;

    if (!tiny->open_count){
        printk(KERN_INFO "port was not open"); 
	printk(KERN_INFO "tiny open count is %d", tiny->open_count); 
	return retval;
	}

    for (i = 0; i < count; ++i)
    {
        printk( KERN_DEBUG " %d %c ", i, buffer[i]);  
    }
	
	tmp_buf = kmalloc(count, GFP_KERNEL);
	if(tmp_buf == NULL){
		return -ENOMEM;
		}
	memset(tmp_buf, 0, count); 
	//memcpy(tmp_buf, buffer, count);
	for (k=0 ; k<count; k++)
	{
		tmp_buf[k] = buffer[k];
	}
        for (j = 0; j < count; ++j)
    	 {
            printk( KERN_DEBUG " %d %c ", j, tmp_buf[j]);        
         }
	printk("\n");

	if (tmp_buf[count-1] == '\n'){
		datalen = count - 1;
		}
		else {
		datalen = count;
		}
	out_digest = kmalloc(256, GFP_KERNEL);
	if(out_digest<0)
		return -ENOMEM;
	alg = crypto_alloc_shash(hash_alg_name, 0, 0);
        if(IS_ERR(alg)){
             pr_info("can't alloc alg %s\n", hash_alg_name);
             return PTR_ERR(alg);
           }
        calc_hash(alg, tmp_buf, datalen, out_digest);
        crypto_free_shash(alg);
	for ( l = 0; l < 32; ++l)
            { 
		sprintf((buf + l * 2), "%x", out_digest[l]) ;
		}
	
            for (i = 0; i < 64; ++i)
            {   
                if (!tty_buffer_request_room(tty->port, 1))
                    tty_flip_buffer_push(tty->port);
                tty_insert_flip_char(tty->port, buf[i], TTY_NORMAL);
            }
            tty_flip_buffer_push(tty->port);

	retval = count;
    return retval;
}

static int tiny_write_room(struct tty_struct *tty) 
{
    struct tiny_serial *tiny = tty->driver_data;
    int room = -EINVAL;

    pr_info("%s\n", __func__);

    if (!tiny)
        return -ENODEV;

    if (!tiny->open_count) {
        /* port was not opened */
        goto exit;
    }

    /* calculate how much room is left in the device */
    room = 255;

exit:
    return room;
}

static int tiny_install(struct tty_driver *driver, struct tty_struct *tty)
{
    int retval = -ENOMEM;

    pr_info("%s\n", __func__);

    tty->port = kmalloc(sizeof *tty->port, GFP_KERNEL);
    if (!tty->port)
        goto err;

    tty_init_termios(tty);
    driver->ttys[0] = tty;

    tty_port_init(tty->port);
    tty_buffer_set_limit(tty->port, 8192);
    tty_driver_kref_get(driver);
    tty->count++;   

    return 0;

err:
    pr_info("%s - err\n", __func__);
    kfree(tty->port);
    return retval;
}

static struct tty_operations serial_ops = {
    .open = tiny_open,
    .close = tiny_close,
    .write = tiny_write,
    .write_room = tiny_write_room,
    .install        = tiny_install,
};

struct tty_driver *tiny_tty_driver;

static int __init tiny_init(void)
{
    int retval;

    pr_info("%s\n", __func__);  

    /* allocate the tty driver */
    tiny_tty_driver = alloc_tty_driver(TINY_TTY_MINORS);
    if (!tiny_tty_driver)
        return -ENOMEM;

    /* initialize the tty driver */
    tiny_tty_driver->owner = THIS_MODULE;
    tiny_tty_driver->driver_name = "tiny_tty";
    tiny_tty_driver->name = "ttyM";
    tiny_tty_driver->major = TINY_TTY_MAJOR,
    tiny_tty_driver->type = TTY_DRIVER_TYPE_SYSTEM,
    tiny_tty_driver->subtype = SERIAL_TYPE_NORMAL,
    tiny_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV | TTY_DRIVER_UNNUMBERED_NODE,
    tiny_tty_driver->init_termios = tty_std_termios;
    tiny_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    tiny_tty_driver->init_termios.c_lflag &= ~ECHO;
    tiny_tty_driver->init_termios.c_oflag &= ~(OPOST);
    tty_set_operations(tiny_tty_driver, &serial_ops);

    /* register the tty driver */
    retval = tty_register_driver(tiny_tty_driver);
    if (retval) {
        printk(KERN_ERR "failed to register tty driver");
        put_tty_driver(tiny_tty_driver);
        return retval;
    }

    tty_register_device(tiny_tty_driver, 0, NULL);

    //tiny_install(tiny_tty_driver, tiny_table[0]->tty);
    return retval;
}

static void __exit tiny_exit(void)
{
    pr_info("%s\n", __func__); 

    tty_unregister_device(tiny_tty_driver, 0);
    tty_unregister_driver(tiny_tty_driver);

    if (tiny_serial) {
        /* close the port */
        while (tiny_serial->open_count)
            --tiny_serial->open_count;

        kfree(tiny_serial);
        tiny_serial = NULL;
    }
}

module_init(tiny_init);
module_exit(tiny_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("TTY driver");
MODULE_LICENSE("GPL");
