struct vma {
	struct spinlock lk;
	uint64 bgAddr;
	uint64 edAddr;
	uint64 flags;
	uint64 prot;
	uint64 offset;
	struct file *f;
	struct vma *next;
};

extern const uint64 BAD_ADDR;
