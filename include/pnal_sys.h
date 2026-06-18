#ifndef PNAL_SYS_H
#define PNAL_SYS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "pnal_config.h"

#define PNAL_IF_NAME_MAX_SIZE 16

typedef struct pnal_cfg
{
   char     if_name[PNAL_IF_NAME_MAX_SIZE];
   uint32_t ip_addr;
   uint32_t netmask;
   uint32_t gateway;
   bool     use_dhcp;
} pnal_cfg_t;

#endif /* PNAL_SYS_H */
