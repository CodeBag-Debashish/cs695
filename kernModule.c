#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <asm/unistd.h>
#include <linux/semaphore.h>
#include <asm/cacheflush.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/fs.h>

extern long (*address)(int *,char **);

asmlinkage void kill_sample_process(struct task_struct *task) {
 
 int signum = SIGKILL;
 struct siginfo info;
 memset(&info,0,sizeof(struct siginfo));
 info.si_signo = signum;
 int ret = send_sig_info(signum,&info,task);
 if(ret < 0) {
  printk(KERN_INFO "error sending signal\n"); 
 }else {
  printk("sample process pid = %d is terminated\n",task->pid);
 }
}

asmlinkage pte_t *walk_page_table(struct mm_struct *mm, unsigned long addr) {
 pgd_t *pgd;
 pud_t *pud;
 pmd_t *pmd;
 pte_t *ptep;
 pgd = pgd_offset(mm, addr);
 if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
  return NULL;
 pud = pud_offset((p4d_t *)pgd, addr);
 if (pud_none(*pud) || unlikely(pud_bad(*pud)))
  return NULL;
 pmd = pmd_offset(pud, addr);
 if (pmd_none(*pmd))
  return NULL;
 ptep = pte_offset_map(pmd, addr);
 return ptep;
}

asmlinkage long find_VSS(struct task_struct *task) {
 
 char buf[500];
 long count;
 long total_vm_area=0;
 struct mm_struct *mm;
 struct vm_area_struct *vma;
 unsigned long inode;
 mm = task->mm;

 printk("Heap\t\t\t= [%lx]\tend address = [%lx]\n",mm->start_brk,mm->brk);
 printk("Data Segment\t\t= [%lx]\tend address = [%lx]\n",mm->start_data,mm->end_data);
 printk("Code Segment\t\t= [%lx]\tend address = [%lx]\n",mm->start_code,mm->end_code);
 count = 0;

 printk(KERN_INFO "\n\n");

 for(vma = mm->mmap ; vma ; vma = vma->vm_next) {
  count++;
  buf[0]='\0';
  if(vma->vm_file)
   strcpy(buf, vma->vm_file->f_path.dentry->d_iname);
  total_vm_area += (vma->vm_end - vma->vm_start)/1024; 
  printk("\t%lu = [%p-%p] \t VSS=%luKB \t %s\n",
    count,
    vma->vm_start,
    vma->vm_end,
    (vma->vm_end - vma->vm_start)/1024,
    buf);
 }
 printk(KERN_INFO "\n\n");
 return total_vm_area;
}

asmlinkage long find_RSS(struct task_struct *task) {

 long count;
 long total_present=0;
 long present=0;
 unsigned long page_addr;
 char buf[500];
 struct mm_struct *mm;
 struct vm_area_struct *vma;
 unsigned long inode;
 pte_t *pte;
 
 mm = task->mm;
 printk(KERN_INFO "\n\n");
 count = 0;
 for(vma = mm->mmap ; vma ; vma = vma->vm_next) { 
  count++;
  present=0;
  inode=0;
  buf[0]='\0';
  if(vma->vm_file)
   strcpy(buf, vma->vm_file->f_path.dentry->d_iname);
  for(page_addr = vma->vm_start ; page_addr < (vma->vm_end) ; page_addr += PAGE_SIZE) {
   pte = walk_page_table(mm,page_addr);
   if(pte && pte_present(*pte)) {
    present += PAGE_SIZE;
   }
  }
  total_present += present;
  printk("\t%lu = [%p-%p] \t RSS=%luKB \t %s\n",
    count,
    vma->vm_start,
    vma->vm_end,
    present/1024,
    buf);
 }
 printk(KERN_INFO "\n\n");
 return total_present/1024;
}

asmlinkage long find_WSS(struct task_struct *task) {
 long value = 0;
 
 return value;
}

asmlinkage long print_all_parents(struct task_struct *task) {
 
 struct task_struct *prev;
 printk(KERN_INFO "Printing the ancestors ... \n");
 do {
  prev = task;
  if(task->parent != NULL) {
   printk(KERN_INFO "\n%s :: %d\n",task->parent->comm , task->parent->pid);
  }
  task = task->parent;
 }while(prev->pid != 0);
 
 printk(KERN_INFO "\nPrinting the sibling ... \n");
 struct task_struct *task_temp;
 struct list_head *list;
 list_for_each(list,&task->sibling) {
  task_temp = list_entry(list,struct task_struct,sibling);
  printk(KERN_INFO "%s :: %d\n",task_temp->comm,task_temp->pid);
 }
 return 0;
}

asmlinkage long hook_func(int *arg1,char **arg2) {
 printk(KERN_INFO "Kernel Hacked You can do all kinds of funny stuff here !\n"); 
 struct task_struct *task;
 long vss,wss,rss;
 pid_t _pid = (pid_t)arg1[0];
 int found = 0; 
 for_each_process(task) {
  if(task->pid == _pid) {
   found = 1;
   break;
  } 
 }
 if(found) {
  
  //msleep(2000);
  
  printk(KERN_INFO "process name = %s",task->comm);

  vss = find_VSS(task);
  rss = find_RSS(task);
  wss = find_WSS(task);
  
  //printk(KERN_INFO "vss = %ld",vss);
  //printk(KERN_INFO "rss = %ld",rss);
  //printk(KERN_INFO "wss = %ld",wss);
  

  //arg1[0] = _pid;
  arg1[1] = vss;
  arg1[2] = rss;
  arg1[3] = wss;

  //print_all_parents(task);

 }else {
  printk(KERN_INFO "process not found\n");
 }

 //kill_sample_process(task);
 return 0;
}

static int init1_module_func(int _pid) {
 address = hook_func;
 printk(KERN_INFO "module loaded ! function placed at location =  %p \n",address);
 return 0;
}

static void exit1_module_func(void) {
 address = NULL;
 printk(KERN_INFO "module unloaded ! address var is set to NULL\n");
}

module_init(init1_module_func);
module_exit(exit1_module_func);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DEBASHISH DEKA");
MODULE_VERSION("0.1");