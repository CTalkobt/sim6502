#include "memory.h"
#include "memory_types.h"
#include <string.h>
#include <stdlib.h>

unsigned char mem_read_phys(memory_t *mem, unsigned int phys) {
	if (phys < 0x10000) {
		uint8_t val;
		if (mem->io_handlers[phys] && mem->io_handlers[phys]->io_read(mem, (uint16_t)phys, &val))
			return val;
		return mem->mem[phys];
	}
	unsigned int page = phys >> FAR_PAGE_SHIFT;
	unsigned int off  = phys & (FAR_PAGE_SIZE - 1);
	if (!mem->far_pages[page])
		return 0;
	return mem->far_pages[page][off];
}

unsigned char mem_read_phys(const memory_t *mem, unsigned int phys) {
    return mem_read_phys(const_cast<memory_t*>(mem), phys);
}

unsigned char mem_peek(memory_t *mem, uint16_t addr) {
    /* Overlay scan: ROM takes priority, same as mem_read but side-effect-free. */
    for (int i = 0; i < mem->overlay_count; i++) {
        const mem_overlay_t *ov = &mem->overlays[i];
        if (!ov->active || !ov->cpu_visible || !ov->data) continue;
        if ((uint32_t)addr >= ov->phys_base && (uint32_t)addr < ov->phys_base + ov->size)
            return ov->data[(uint32_t)addr - ov->phys_base];
    }
    uint8_t val;
    if (mem->io_handlers[addr] && mem->io_handlers[addr]->io_peek(mem, addr, &val))
        return val;
    return mem->mem[addr];
}

unsigned char mem_peek(const memory_t *mem, uint16_t addr) {
    return mem_peek(const_cast<memory_t*>(mem), addr);
}

void mem_write_phys(memory_t *mem, unsigned int phys, unsigned char val) {
	if (phys < 0x10000) {
		if (mem->io_handlers[phys] && mem->io_handlers[phys]->io_write(mem, (uint16_t)phys, val))
			return;
		mem->mem[phys] = val;
		return;
	}
	unsigned int page = phys >> FAR_PAGE_SHIFT;
	unsigned int off  = phys & (FAR_PAGE_SIZE - 1);
	if (!mem->far_pages[page]) mem->far_pages[page] = new unsigned char[FAR_PAGE_SIZE]();
	mem->far_pages[page][off] = val;
}


unsigned char mem_read(memory_t *mem, unsigned short addr) {
	unsigned int block = (unsigned int)addr >> 13;
	if (mem->map_offset[block]) {
		unsigned int phys = ((unsigned int)addr + mem->map_offset[block]) & 0xFFFFF;
		return mem_read_phys(mem, phys);
	}
	/* ROM overlay scan: check CPU-visible active overlays before I/O or RAM. */
	for (int i = 0; i < mem->overlay_count; i++) {
		const mem_overlay_t *ov = &mem->overlays[i];
		if (!ov->active || !ov->cpu_visible || !ov->data) continue;
		if ((uint32_t)addr >= ov->phys_base && (uint32_t)addr < ov->phys_base + ov->size)
			return ov->data[(uint32_t)addr - ov->phys_base];
	}
	uint8_t val;
	if (mem->io_handlers[addr] && mem->io_handlers[addr]->io_read(mem, addr, &val))
		return val;
	return mem->mem[addr];
}

void mem_write(memory_t *mem, unsigned short addr, unsigned char val) {
	unsigned int block = (unsigned int)addr >> 13;
	if (mem->mem_writes < 256) {
		mem->mem_addr[mem->mem_writes] = addr;
		mem->mem_old_val[mem->mem_writes] = mem_read(mem, addr);
		mem->mem_val[mem->mem_writes] = val;
	}
	mem->mem_writes++;
	if (mem->map_offset[block]) {
		unsigned int phys = ((unsigned int)addr + mem->map_offset[block]) & 0xFFFFF;
		mem_write_phys(mem, phys, val);
	} else {
		if (mem->io_handlers[addr] && mem->io_handlers[addr]->io_write(mem, addr, val))
			return;
		mem_write_phys(mem, addr, val);
	}
}

unsigned char far_mem_read(memory_t *m, unsigned int addr) {
	return mem_read_phys(m, addr);
}

void far_mem_write(memory_t *m, unsigned int addr, unsigned char val) {
	if (addr < 0x10000) {
		if (m->mem_writes < 256) {
			m->mem_addr[m->mem_writes] = (unsigned short)addr;
			m->mem_old_val[m->mem_writes] = far_mem_read(m, addr);
			m->mem_val[m->mem_writes] = val;
		}
		m->mem_writes++;
	}
	mem_write_phys(m, addr, val);
}

void mem_free_far_pages(memory_t *mem) {
    if (!mem) return;
    for (int i = 0; i < FAR_NUM_PAGES; i++) {
        if (mem->far_pages[i]) {
            delete[] mem->far_pages[i];
            mem->far_pages[i] = NULL;
        }
    }
}

int mem_overlay_add(memory_t *mem, uint32_t phys_base, uint32_t size,
                    uint8_t *data, rom_type_t type,
                    int cpu_visible, int vic_visible, int active) {
    if (!mem || mem->overlay_count >= MAX_OVERLAYS) return -1;
    int idx = mem->overlay_count++;
    mem->overlays[idx].phys_base   = phys_base;
    mem->overlays[idx].size        = size;
    mem->overlays[idx].data        = data;
    mem->overlays[idx].type        = type;
    mem->overlays[idx].active      = active;
    mem->overlays[idx].cpu_visible = cpu_visible;
    mem->overlays[idx].vic_visible = vic_visible;
    mem->overlays[idx].owns_data   = 0;
    return idx;
}

void mem_overlay_set_active(memory_t *mem, int idx, int active) {
    if (!mem || idx < 0 || idx >= mem->overlay_count) return;
    mem->overlays[idx].active = active;
}

int mem_overlay_find(memory_t *mem, uint32_t phys_base) {
    if (!mem) return -1;
    for (int i = 0; i < mem->overlay_count; i++) {
        if (mem->overlays[i].phys_base == phys_base)
            return i;
    }
    return -1;
}

void mem_overlay_clear_all(memory_t *mem) {
    if (!mem) return;
    for (int i = 0; i < mem->overlay_count; i++) {
        if (mem->overlays[i].owns_data && mem->overlays[i].data) {
            free(mem->overlays[i].data);
            mem->overlays[i].data = NULL;
        }
    }
    mem->overlay_count = 0;
}
