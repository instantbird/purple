/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <mozilla/mozalloc.h>
#include <nsCOMPtr.h>
#include <nsIMemoryReporter.h>
#include <nsStringAPI.h>
#include <nsServiceManagerUtils.h>

static size_t allocated_size = 0;

static gpointer g_moz_malloc(gsize n_bytes)
{
  gpointer ptr = moz_xmalloc(n_bytes);
  allocated_size += moz_malloc_usable_size(ptr);
  return ptr;
}

static gpointer g_moz_realloc(gpointer ptr, gsize n_bytes)
{
  allocated_size -= moz_malloc_usable_size(ptr);
  ptr = moz_xrealloc(ptr, n_bytes);
  allocated_size += moz_malloc_usable_size(ptr);
  return ptr;
}

static void g_moz_free(gpointer ptr)
{
  allocated_size -= moz_malloc_usable_size(ptr);
  moz_free(ptr);
}

static gpointer g_moz_calloc(gsize n_blocks, gsize n_block_bytes)
{
  gpointer ptr = moz_xcalloc(n_blocks, n_block_bytes);
  allocated_size += moz_malloc_usable_size(ptr);
  return ptr;
}

static gpointer g_moz_try_malloc(gsize n_bytes)
{
  gpointer ptr = moz_malloc(n_bytes);
  allocated_size += moz_malloc_usable_size(ptr);
  return ptr;
}

static gpointer g_moz_try_realloc(gpointer ptr, gsize n_bytes)
{
  allocated_size -= moz_malloc_usable_size(ptr);
  ptr = moz_realloc(ptr, n_bytes);
  allocated_size += moz_malloc_usable_size(ptr);
  return ptr;
}

static GMemVTable allocator =
{
  g_moz_malloc,
  g_moz_realloc,
  g_moz_free,
  g_moz_calloc,
  g_moz_try_malloc,
  g_moz_try_realloc
};

static int64_t getAllocatedMemory()
{
  return allocated_size;
}

NS_THREADSAFE_MEMORY_REPORTER_IMPLEMENT(glib, "explicit/purple/libraries/glib",
                                        KIND_HEAP, UNITS_BYTES, getAllocatedMemory,
                                        "Memory allocated by glib and libpurple.");

void init_glib_memory_reporter()
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");
  if (mgr) {
    if (NS_SUCCEEDED(mgr->RegisterReporter(new NS_MEMORY_REPORTER_NAME(glib))))
      g_mem_set_vtable(&allocator);
  }
}


static size_t allocated_xml_size = 0;

static void moz_xml_free(void *mem)
{
  allocated_xml_size -= moz_malloc_usable_size(mem);
  moz_free(mem);
}

static void *moz_xml_malloc(size_t size)
{
  void *mem = moz_malloc(size);
  allocated_xml_size += moz_malloc_usable_size(mem);
  return mem;
}

static void *moz_xml_realloc(void *mem, size_t size)
{
  allocated_xml_size -= moz_malloc_usable_size(mem);
  mem = moz_realloc(mem, size);
  allocated_xml_size += moz_malloc_usable_size(mem);
  return mem;
}

static char *moz_xml_strdup(const char *str)
{
  char *dup = moz_strdup(str);
  allocated_xml_size += moz_malloc_usable_size(dup);
  return dup;
}

static int64_t getAllocatedXMLMemory()
{
  return allocated_xml_size;
}

NS_THREADSAFE_MEMORY_REPORTER_IMPLEMENT(xml2, "explicit/purple/libraries/xml2",
                                        KIND_HEAP, UNITS_BYTES, getAllocatedXMLMemory,
                                        "Memory allocated by libxml2 for libpurple.");

void init_libxml2_memory_reporter()
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");
  if (mgr) {
    if (NS_SUCCEEDED(mgr->RegisterReporter(new NS_MEMORY_REPORTER_NAME(xml2))))
      xmlMemSetup(moz_xml_free, moz_xml_malloc, moz_xml_realloc, moz_xml_strdup);
  }
}
