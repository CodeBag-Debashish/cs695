#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h> 
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sysfs.h>
#include <asm/types.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/stat.h>

#define PROCFS_MAX_SIZE		1024
#define PROCFS_NAME_PROD 	"producer"
#define PROCFS_NAME_CON 	"consumer"
#define QUEUE_SIZE			100

static struct proc_dir_entry *consumer_entry;
static struct proc_dir_entry *producer_entry;

static char procfs_buffer[PROCFS_MAX_SIZE];
static unsigned long procfs_buffer_size = 1024;

int no_of_read = 0;
int no_of_write = 0;
int no_of_bytes_read = 0;
int no_of_byte_written = 0;

static int q_current_size;
static int q_capacity;

typedef struct  {
	int elem_size; // size in BYTES
	int ptr;
	void * elem; // anything can be pushed
} q_element;

typedef struct{
    q_element * elem;
    struct list_head list;
} queue;

int granularity = 3;

static struct queue *_node;
struct list_head *queue_head;


int is_empty(void) {
	return (q_current_size == 0);
}

void enqueue(q_element * elem) {
    if(q_current_size == q_capacity){
		printk(KERN_INFO "Queue is Full. No element added.\n");
		kfree(elem); // we just allocated something in the hope of free space
		return;
	} else{
		q_current_size++;
		queue * new_node = (queue*)kmalloc(sizeof(queue),GFP_KERNEL);
		printk(KERN_INFO "\t %d Bytes memory allocated for a new queue_node",sizeof(queue));	
		new_node->elem = elem; // elem must be pre allocated
		list_add_tail(&(new_node->list), queue_head);
    }
}

void dequeue(void) {
	if(q_current_size == 0) {
		printk(KERN_INFO "Queue is Empty.\n");
		return;
	}else {
		q_current_size--;
		queue *tmp = list_entry(queue_head->next, queue, list);
		list_del(queue_head->next);

		// free whatever we have allocated		
		kfree(tmp->elem->elem);
		kfree(tmp->elem);
		kfree(tmp);
	}
}

q_element * front(void) {
	if(q_current_size == 0) {
		printk(KERN_INFO "Queue is Empty\n");
		return NULL; // return NULL in case of the queue is empty
	}
	struct list_head * first = queue_head->next;
	queue * first_element = list_entry(first, queue, list);
	return first_element->elem; // send the q_element object
}

q_element * tail(void) { 
	if(q_current_size == 0) {
		printk(KERN_INFO "Queue is Empty.\n");
		return NULL; // return NULL in the case of the empty queue
	}
	struct list_head * last = queue_head->prev;
	queue * last_element = list_entry(last, queue, list);
	return last_element->elem; 
}

static struct kobject *example_kobject;

static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return sprintf(buf, "\tno of write = \t\t %d \t no of read = \t\t %d\n \tno of bytes read = \t %d \t no of bytes written = \t %d\n", no_of_write,no_of_read,no_of_bytes_read,no_of_byte_written);
}

static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count) {
	sscanf(buf, "%du", &granularity);
	return count;
}

#undef VERIFY_OCTAL_PERMISSIONS
#define VERIFY_OCTAL_PERMISSIONS(perms) (perms)
static struct kobj_attribute foo_attribute =__ATTR(foo, 0666, foo_show,foo_store);

static ssize_t consume_item(struct file *fp, char *buf, size_t len, loff_t * off) {	
	
	printk(KERN_INFO "procfile_read (/proc/%s) called\n", PROCFS_NAME_CON);
	printk(KERN_INFO "buffer len = %d\n for this invocation of consumer", strlen(buf));
	static int done = 0; 
	if(done) {
		done = 0;
		return done;
	}
	no_of_read++;
	int total_bytes_read = 0;
	int full = 0;
	
	q_element *front_element = front(); // ok
	

	if(front_element != NULL) {
		int byte_count = 0;
		char *temp_store;

		if( granularity == 0 ) {

			temp_store = kmalloc( (front_element->elem_size) + 2 , GFP_KERNEL);
			memcpy(temp_store , (char *)(front_element->elem) + (front_element->ptr) , (front_element->elem_size) );
			temp_store[ (front_element->elem_size) ] = '\0';
			sprintf(buf, "\t %s\n",temp_store);
			kfree(temp_store);
			int res = (front_element->elem_size) + 1; 
			done = 1;
			dequeue();
			return res + 2;
		}

		if( (front_element->elem_size) >= granularity ) {
			printk(KERN_INFO "accomodated \n");
			(front_element->elem_size) -= granularity;
			temp_store = kmalloc(granularity + 1 , GFP_USER);
			memcpy(temp_store,(char *)(front_element->elem) +  (front_element->ptr) , granularity );
			temp_store[granularity] = '\0';
			sprintf(buf, "\t %s\n",temp_store);
			kfree(temp_store);
			no_of_bytes_read += granularity;
			front_element->ptr += granularity; 
			full = 1;
			byte_count += granularity;
			if( (front_element->elem_size) == 0 ) {
				dequeue();
			}

		}else {
			int still_req = granularity - (front_element->elem_size);
			temp_store = kmalloc(granularity + 1, GFP_USER);
			int idx = 0;
			memcpy(temp_store,(char *)(front_element->elem) + (front_element->ptr) , ( front_element->elem_size ) );			
			byte_count += (front_element->elem_size);
			dequeue();		
			while( !is_empty()) {
				front_element = front();
				if(front_element != NULL) {	
					if( (front_element->elem_size) >= still_req ) {
						(front_element->elem_size) -= still_req;
						memcpy(temp_store + byte_count ,(char *)(front_element->elem) + (front_element->ptr), still_req );
						(front_element->ptr) += still_req;
						byte_count += still_req;
						full = 1;
						if( (front_element->elem_size) == 0) {
							dequeue();
						}
						break;
					}else {
						if( (front_element->elem_size) > 0) {	
							still_req -= (front_element->elem_size);
							memcpy(temp_store + byte_count , (char *)(front_element->elem), ( front_element->elem_size ) );
							byte_count += (front_element->elem_size); 	
						}
						dequeue();
					}
				}else {		
					dequeue();
				}
			}

			printk(KERN_INFO "klsnlksnflksndlksndlk\n");

			temp_store[byte_count] = '\0';
			sprintf(buf, "\t%s\n",temp_store);
			kfree(temp_store);
			no_of_bytes_read += byte_count;
			total_bytes_read = byte_count + 2;
		}
		done = 1;
	}else {
		printk(KERN_INFO "NULL returned !!\n");

		return 0;
	}
	return (full == 1)?strlen(buf) + 1 : total_bytes_read;
}

static ssize_t produce_item(struct file *fp, const char *buf, size_t len, loff_t * off) {	

	if(len > PROCFS_MAX_SIZE) { 
		return -EFAULT; 
	}
	int _size = len - 1; 
	printk(KERN_INFO "\t len == %d bytes written to FIFO", len);
	no_of_write++;
	no_of_byte_written += _size;
	void * temp_store = kmalloc( _size , GFP_KERNEL );
	printk(KERN_INFO "\t %d Bytes memory allocated for a new raw data element",_size);
	if(copy_from_user(temp_store, buf, _size)) { 	
		return -EFAULT; 
	}
	q_element *new_elem = (q_element *)kmalloc(sizeof(q_element),GFP_KERNEL);
	printk(KERN_INFO "\t %d Bytes memory allocated for a new queue_element",sizeof(q_element));	
	new_elem->elem = temp_store;
	new_elem->elem_size = _size;
	new_elem->ptr = 0;
	enqueue(new_elem);
	return len;
}

static struct file_operations consumer_fops = { 
	.owner	=	THIS_MODULE, 
	.read 	=	consume_item, 
};

static struct file_operations producer_fops = { 
	.owner	=	THIS_MODULE, 
	.write 	=	produce_item, 
};

static int init1_module_func(void) {

	printk(KERN_INFO "Module initialized\n");
	char *str = "Welcome";
	sprintf(procfs_buffer, "%s\n", str);
	/*
	#define S_IRUGO			(S_IRUSR|S_IRGRP|S_IROTH)
	#define S_IWUGO			(S_IWUSR|S_IWGRP|S_IWOTH)
	
	rare stuff :D
	https://elixir.bootlin.com/linux/v4.14.13/source/include/linux/stat.h#L11
	*/
 	producer_entry = proc_create( PROCFS_NAME_PROD, S_IWUGO, NULL, &producer_fops); 
 	if(producer_entry == NULL) {	
 		printk(KERN_ALERT "Error: Could not initialize %s\n", PROCFS_NAME_PROD); 
 	}
 	
 	consumer_entry = proc_create( PROCFS_NAME_CON, S_IRUGO, NULL, &consumer_fops);
	if(consumer_entry == NULL) {	
 		printk(KERN_ALERT "Error: Could not initialize %s\n", PROCFS_NAME_CON); 
 	}

 	queue_head = (struct list_head *)kmalloc(sizeof(struct list_head),GFP_KERNEL);
 	printk(KERN_INFO "\t %d Bytes memory allocated for first list_head",sizeof(struct list_head));	
 	INIT_LIST_HEAD(queue_head);
 	q_capacity = QUEUE_SIZE;
 	q_current_size = 0;
	
	/*struct kobject {
		const char		*name;
		struct list_head	entry;
		struct kobject		*parent;
		struct kset		*kset;
		struct kobj_type	*ktype;
		struct sysfs_dirent	*sd;
		struct kref		kref;
		unsigned int state_initialized:1;
		unsigned int state_in_sysfs:1;
		unsigned int state_add_uevent_sent:1;
		unsigned int state_remove_uevent_sent:1;
		unsigned int uevent_suppress:1;
	};*/

	example_kobject = kobject_create_and_add("kobject_example",kernel_kobj);
	if(!example_kobject) {
		return -ENOMEM;
	}
	int error = sysfs_create_file(example_kobject, &foo_attribute.attr);
	if(error) {
		printk(KERN_INFO "failed to create the foo file in /sys/kernel/kobject_example\n");
	}
	return 0;
}

static void exit1_module_func(void)  {  

	remove_proc_entry(PROCFS_NAME_CON, NULL);
	remove_proc_entry(PROCFS_NAME_PROD, NULL);
	kobject_put(example_kobject);
	printk(KERN_INFO "/sys/kernel/kobject_example removed\n");
	printk(KERN_INFO "/proc/%s removed\n", PROCFS_NAME_CON);
	printk(KERN_INFO "/proc/%s removed\n", PROCFS_NAME_PROD);
	printk(KERN_INFO "module unloaded\n");
	



	kfree(queue_head);


}

module_init(init1_module_func);
module_exit(exit1_module_func);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("DEBASHISH DEKA");
MODULE_VERSION("0.1");
