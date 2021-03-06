/* The LibVMI Library is an introspection library that simplifies access to
 * memory in a target virtual machine or in a file containing a dump of
 * a system's physical memory.  LibVMI is based on the XenAccess Library.
 *
 * Copyright 2011 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
 * retains certain rights in this software.
 *
 * Author: Bryan D. Payne (bdpayne@acm.org)
 *
 * This file is part of LibVMI.
 *
 * LibVMI is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * LibVMI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with LibVMI.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libvmi.h"
#include "private.h"
#include "driver/interface.h"
#include <string.h>
#include <wchar.h>
#include <errno.h>

///////////////////////////////////////////////////////////
// Classic read functions for access to memory

// Reads memory at a guest's physical address
size_t
vmi_read_pa(
    vmi_instance_t vmi,
    addr_t paddr,
    void *buf,
    size_t count)
{
    //TODO not sure how to best handle this with respect to page size.  Is this hypervisor dependent?
    //  For example, the pfn for a given paddr should vary based on the size of the page where the
    //  paddr resides.  However, it is hard to know the page size from just the paddr.  For now, just
    //  assuming 4k pages and doing the read from there.

    unsigned char *memory = NULL;
    addr_t phys_address = 0;
    addr_t pfn = 0;
    addr_t offset = 0;
    size_t buf_offset = 0;

    while (count > 0) {
        size_t read_len = 0;

        /* access the memory */
        phys_address = paddr + buf_offset;
        pfn = phys_address >> vmi->page_shift;
        offset = (vmi->page_size - 1) & phys_address;
        memory = vmi_read_page(vmi, pfn);
        if (NULL == memory) {
            return buf_offset;
        }

        /* determine how much we can read */
        if ((offset + count) > vmi->page_size) {
            read_len = vmi->page_size - offset;
        }
        else {
            read_len = count;
        }

        /* do the read */
        memcpy(((char *) buf) + (addr_t) buf_offset,
               memory + (addr_t) offset, read_len);

        /* set variables for next loop */
        count -= read_len;
        buf_offset += read_len;
    }

    return buf_offset;
}

size_t
vmi_read_va(
    vmi_instance_t vmi,
    addr_t vaddr,
    vmi_pid_t pid,
    void *buf,
    size_t count)
{
    unsigned char *memory = NULL;
    addr_t paddr = 0;
    addr_t pfn = 0;
    addr_t offset = 0;
    size_t buf_offset = 0;

    if (NULL == buf) {
        dbprint(VMI_DEBUG_READ, "--%s: buf passed as NULL, returning without read\n",
                __FUNCTION__);
        return 0;
    }

    while (count > 0) {
        size_t read_len = 0;

        if (pid) {
            paddr = vmi_translate_uv2p(vmi, vaddr + buf_offset, pid);
        }
        else {
            paddr = vmi_translate_kv2p(vmi, vaddr + buf_offset);
        }

        if (!paddr) {
            return buf_offset;
        }

        /* access the memory */
        pfn = paddr >> vmi->page_shift;
        offset = (vmi->page_size - 1) & paddr;
        memory = vmi_read_page(vmi, pfn);
        if (NULL == memory) {
            return buf_offset;
        }

        /* determine how much we can read */
        if ((offset + count) > vmi->page_size) {
            read_len = vmi->page_size - offset;
        }
        else {
            read_len = count;
        }

        /* do the read */
        memcpy(((char *) buf) + (addr_t) buf_offset,
               memory + (addr_t) offset, read_len);

        /* set variables for next loop */
        count -= read_len;
        buf_offset += read_len;
    }

    return buf_offset;
}

#if ENABLE_SHM_SNAPSHOT == 1
size_t
vmi_get_dgpma(
    vmi_instance_t vmi,
    addr_t paddr,
    void **buf_ptr,
    size_t count)
{
    return driver_get_dgpma(vmi, paddr, buf_ptr, count);
}

size_t
vmi_get_dgvma(
    vmi_instance_t vmi,
    addr_t vaddr,
    pid_t pid,
    void **buf_ptr,
    size_t count)
{
    return driver_get_dgvma(vmi, vaddr, pid, buf_ptr, count);
}
#endif

size_t
vmi_read_ksym(
    vmi_instance_t vmi,
    char *sym,
    void *buf,
    size_t count)
{
    addr_t vaddr = vmi_translate_ksym2v(vmi, sym);

    if (0 == vaddr) {
        dbprint(VMI_DEBUG_READ, "--%s: vmi_translate_ksym2v failed for '%s'\n",
                __FUNCTION__, sym);
        return 0;
    }
    return vmi_read_va(vmi, vaddr, 0, buf, count);
}

///////////////////////////////////////////////////////////
// Easy access to physical memory
static status_t
vmi_read_X_pa(
    vmi_instance_t vmi,
    addr_t paddr,
    void *value,
    int size)
{
    size_t len_read = vmi_read_pa(vmi, paddr, value, size);

    if (len_read == size) {
        return VMI_SUCCESS;
    }
    else {
        return VMI_FAILURE;
    }
}

status_t
vmi_read_8_pa(
    vmi_instance_t vmi,
    addr_t paddr,
    uint8_t * value)
{
    return vmi_read_X_pa(vmi, paddr, value, 1);
}

status_t
vmi_read_16_pa(
    vmi_instance_t vmi,
    addr_t paddr,
    uint16_t * value)
{
    return vmi_read_X_pa(vmi, paddr, value, 2);
}

status_t
vmi_read_32_pa(
    vmi_instance_t vmi,
    addr_t paddr,
    uint32_t * value)
{
    return vmi_read_X_pa(vmi, paddr, value, 4);
}

status_t
vmi_read_64_pa(
    vmi_instance_t vmi,
    addr_t paddr,
    uint64_t * value)
{
    return vmi_read_X_pa(vmi, paddr, value, 8);
}

status_t
vmi_read_addr_pa(
    vmi_instance_t vmi,
    addr_t paddr,
    addr_t *value)
{
    if (vmi->page_mode == VMI_PM_IA32E) {
        return vmi_read_64_pa(vmi, paddr, value);
    }
    else {
        uint32_t tmp = 0;
        status_t ret = vmi_read_32_pa(vmi, paddr, &tmp);

        *value = (uint64_t) tmp;
        return ret;
    }
}

char *
vmi_read_str_pa(
    vmi_instance_t vmi,
    addr_t paddr)
{
    char *rtnval = NULL;
    size_t chunk_size = vmi->page_size - ((vmi->page_size - 1) & paddr);
    char *buf = (char *) safe_malloc(chunk_size);

    // read in chunk of data
    if (chunk_size != vmi_read_pa(vmi, paddr, buf, chunk_size)) {
        goto exit;
    }

    // look for \0 character, expand as needed
    size_t len = strnlen(buf, chunk_size);
    size_t buf_size = chunk_size;

    while (len == buf_size) {
        size_t offset = buf_size;

        buf_size += chunk_size;
        buf = realloc(buf, buf_size);
        if (chunk_size !=
            vmi_read_pa(vmi, paddr + offset, buf + offset,
                        chunk_size)) {
            goto exit;
        }
        len = strnlen(buf, buf_size);
    }

    rtnval = (char *) safe_malloc(len + 1);
    memcpy(rtnval, buf, len);
    rtnval[len] = '\0';

exit:
    free(buf);
    return rtnval;
}

///////////////////////////////////////////////////////////
// Easy access to virtual memory
static status_t
vmi_read_X_va(
    vmi_instance_t vmi,
    addr_t vaddr,
    vmi_pid_t pid,
    void *value,
    int size)
{
    size_t len_read = vmi_read_va(vmi, vaddr, pid, value, size);

    if (len_read == size) {
        return VMI_SUCCESS;
    }
    else {
        return VMI_FAILURE;
    }
}

status_t
vmi_read_8_va(
    vmi_instance_t vmi,
    addr_t vaddr,
    vmi_pid_t pid,
    uint8_t * value)
{
    return vmi_read_X_va(vmi, vaddr, pid, value, 1);
}

status_t
vmi_read_16_va(
    vmi_instance_t vmi,
    addr_t vaddr,
    vmi_pid_t pid,
    uint16_t * value)
{
    return vmi_read_X_va(vmi, vaddr, pid, value, 2);
}

status_t
vmi_read_32_va(
    vmi_instance_t vmi,
    addr_t vaddr,
    vmi_pid_t pid,
    uint32_t * value)
{
    return vmi_read_X_va(vmi, vaddr, pid, value, 4);
}

status_t
vmi_read_64_va(
    vmi_instance_t vmi,
    addr_t vaddr,
    vmi_pid_t pid,
    uint64_t * value)
{
    return vmi_read_X_va(vmi, vaddr, pid, value, 8);
}

status_t
vmi_read_addr_va(
    vmi_instance_t vmi,
    addr_t vaddr,
    vmi_pid_t pid,
    addr_t *value)
{
    if (vmi->page_mode == VMI_PM_IA32E) {
        return vmi_read_64_va(vmi, vaddr, pid, value);
    }
    else {
        uint32_t tmp = 0;
        status_t ret = vmi_read_32_va(vmi, vaddr, pid, &tmp);

        *value = (uint64_t) tmp;
        return ret;
    }
}

char *
vmi_read_str_va(
    vmi_instance_t vmi,
    addr_t vaddr,
    vmi_pid_t pid)
{
    unsigned char *memory = NULL;
    char *rtnval = NULL;
    addr_t paddr = 0;
    addr_t pfn = 0;
    addr_t offset = 0;
    int len = 0;
    size_t read_len = 0;
    int read_more = 1;

    rtnval = NULL;

    while (read_more) {
        if (pid) {
            paddr = vmi_translate_uv2p(vmi, vaddr + len, pid);
        }
        else {
            paddr = vmi_translate_kv2p(vmi, vaddr + len);
        }

        if (!paddr) {
            return rtnval;
        }

        /* access the memory */
        pfn = paddr >> vmi->page_shift;
        offset = (vmi->page_size - 1) & paddr;
        memory = vmi_read_page(vmi, pfn);
        if (NULL == memory) {
            return rtnval;
        }

        /* Count new non-null characters */
        read_len = 0;
        while (offset + read_len < vmi->page_size) {
            if (memory[offset + read_len] == '\0') {
                read_more = 0;
                break;
            }

            read_len++;
        }

        /* Otherwise, realloc, tack on the '\0' in case of errors and
         * get ready to read the next page.
         */
        rtnval = realloc(rtnval, len + 1 + read_len);
        memcpy(&rtnval[len], &memory[offset], read_len);
        len += read_len;
        rtnval[len] = '\0';
    }

    return rtnval;
}

unicode_string_t *
vmi_read_unicode_str_va(vmi_instance_t vmi, addr_t vaddr, vmi_pid_t pid) {
    unicode_string_t *ret = NULL;
    if (vmi->os_interface && vmi->os_interface->os_read_unicode_struct) {
        ret = vmi->os_interface->os_read_unicode_struct(vmi, vaddr, pid);
    }

    return ret;
}

///////////////////////////////////////////////////////////
// Easy access to memory using kernel symbols
static status_t
vmi_read_X_ksym(
    vmi_instance_t vmi,
    char *sym,
    void *value,
    int size)
{
    size_t len_read = vmi_read_ksym(vmi, sym, value, size);

    if (len_read == size) {
        return VMI_SUCCESS;
    }
    else {
        return VMI_FAILURE;
    }
}

status_t
vmi_read_8_ksym(
    vmi_instance_t vmi,
    char *sym,
    uint8_t * value)
{
    return vmi_read_X_ksym(vmi, sym, value, 1);
}

status_t
vmi_read_16_ksym(
    vmi_instance_t vmi,
    char *sym,
    uint16_t * value)
{
    return vmi_read_X_ksym(vmi, sym, value, 2);
}

status_t
vmi_read_32_ksym(
    vmi_instance_t vmi,
    char *sym,
    uint32_t * value)
{
    return vmi_read_X_ksym(vmi, sym, value, 4);
}

status_t
vmi_read_64_ksym(
    vmi_instance_t vmi,
    char *sym,
    uint64_t * value)
{
    return vmi_read_X_ksym(vmi, sym, value, 8);
}

status_t
vmi_read_addr_ksym(
    vmi_instance_t vmi,
    char *sym,
    addr_t *value)
{
    if (vmi->page_mode == VMI_PM_IA32E) {
        return vmi_read_64_ksym(vmi, sym, value);
    }
    else {
        uint32_t tmp = 0;
        status_t ret = vmi_read_32_ksym(vmi, sym, &tmp);

        *value = (uint64_t) tmp;
        return ret;
    }
}

char *
vmi_read_str_ksym(
    vmi_instance_t vmi,
    char *sym)
{
    addr_t vaddr = vmi_translate_ksym2v(vmi, sym);

    return vmi_read_str_va(vmi, vaddr, 0);
}
