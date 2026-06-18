/**
 * pnet_protocol.h - Profinet Protocol Core Layer
 *
 * Based on document chapter 2.2: Protocol core layer working principles
 * Implements data encapsulation, transmission, reception, and real-time
 * communication mechanisms for Profinet protocol.
 */

#ifndef PNET_PROTOCOL_H
#define PNET_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Profinet EtherType */
#define PNET_ETHERTYPE_PROFINET  0x8892
#define PNET_ETHERTYPE_LLDP      0x88CC

/* Profinet frame types */
#define PNET_FRAME_TYPE_RT       0xFC  /* Real-Time */
#define PNET_FRAME_TYPE_RTC      0x80  /* Real-Time Class */
#define PNET_FRAME_TYPE_DCP      0xFE  /* Discovery and Configuration Protocol */
#define PNET_FRAME_TYPE_ALARM    0xFD  /* Alarm */

/* Maximum sizes */
#define PNET_MAX_FRAME_SIZE      1518
#define PNET_MIN_FRAME_SIZE      64
#define PNET_MAX_PAYLOAD_SIZE    1440
#define PNET_MAX_SLOTS           32
#define PNET_MAX_SUBSLOTS        8
#define PNET_MAX_CR_COUNT        4

/* Communication relationship types */
typedef enum {
    PNET_CR_TYPE_IO_DATA = 0,
    PNET_CR_TYPE_IO_PARAM,
    PNET_CR_TYPE_ALARM,
    PNET_CR_TYPE_ADMIN
} pnet_cr_type_t;

/* Profinet real-time class */
typedef enum {
    PNET_RT_CLASS_UDP = 0,  /* Non real-time (NRT) via UDP */
    PNET_RT_CLASS_RT  = 1,  /* Real-Time (RT) */
    PNET_RT_CLASS_IRT = 2,  /* Isochronous Real-Time (IRT) */
    PNET_RT_CLASS_TSN = 3   /* Time-Sensitive Networking */
} pnet_rt_class_t;

/* Ethernet header */
typedef struct __attribute__((packed)) {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} pnet_eth_header_t;

/* Profinet real-time header */
typedef struct __attribute__((packed)) {
    uint8_t  frame_id_high;
    uint8_t  frame_id_low;
    uint16_t data_length;
    uint16_t alarm_dst_slot;
    uint16_t alarm_dst_subslot;
} pnet_rt_header_t;

/* Profinet frame structure */
typedef struct {
    pnet_eth_header_t eth_header;
    pnet_rt_header_t  rt_header;
    uint8_t           payload[PNET_MAX_PAYLOAD_SIZE];
    size_t            payload_len;
    size_t            total_len;
    pnet_rt_class_t   rt_class;
    uint32_t          cycle_counter;
    bool              valid;
} pnet_frame_t;

/* Communication Relationship (CR) descriptor */
typedef struct {
    pnet_cr_type_t  type;
    pnet_rt_class_t rt_class;
    uint16_t        frame_id;
    uint16_t        send_clock_factor;
    uint16_t        reduction_ratio;
    uint16_t        watchdog_factor;
    uint32_t        cycle_time_us;  /* Cycle time in microseconds */
    bool            active;
} pnet_cr_descriptor_t;

/* IO data descriptor for slot/subslot */
typedef struct {
    uint16_t slot;
    uint16_t subslot;
    uint8_t  input_data[256];
    uint8_t  output_data[256];
    size_t   input_len;
    size_t   output_len;
    bool     data_valid;
} pnet_io_data_t;

/* Protocol statistics */
typedef struct {
    uint64_t frames_sent;
    uint64_t frames_received;
    uint64_t frames_dropped;
    uint64_t crc_errors;
    uint64_t timeout_errors;
    double   avg_latency_us;
    double   max_latency_us;
    double   min_latency_us;
    double   jitter_us;
} pnet_protocol_stats_t;

/* Protocol lifecycle */
int  pnet_protocol_init(void);
void pnet_protocol_shutdown(void);

/* Frame construction and parsing */
int  pnet_frame_build(pnet_frame_t *frame, const pnet_io_data_t *io_data);
int  pnet_frame_parse(const uint8_t *raw, size_t len, pnet_frame_t *frame);
int  pnet_frame_validate(const pnet_frame_t *frame);
size_t pnet_frame_serialize(const pnet_frame_t *frame, uint8_t *buffer, size_t buf_len);

/* Data exchange - periodic (cyclic) */
int  pnet_cyclic_send(pnet_cr_descriptor_t *cr, const pnet_io_data_t *data);
int  pnet_cyclic_receive(pnet_cr_descriptor_t *cr, pnet_io_data_t *data);

/* Data exchange - acyclic */
int  pnet_acyclic_read(uint16_t slot, uint16_t subslot, uint16_t index,
                       void *buffer, size_t buf_len);
int  pnet_acyclic_write(uint16_t slot, uint16_t subslot, uint16_t index,
                        const void *buffer, size_t len);

/* Communication Relationship management */
int  pnet_cr_create(pnet_cr_descriptor_t *cr, pnet_cr_type_t type, pnet_rt_class_t rt_class);
int  pnet_cr_activate(pnet_cr_descriptor_t *cr);
int  pnet_cr_deactivate(pnet_cr_descriptor_t *cr);
int  pnet_cr_destroy(pnet_cr_descriptor_t *cr);

/* Statistics */
int  pnet_protocol_get_stats(pnet_protocol_stats_t *stats);
void pnet_protocol_reset_stats(void);

/* Utility functions */
uint16_t pnet_frame_id(uint16_t high, uint16_t low);
const char* pnet_rt_class_str(pnet_rt_class_t rt_class);
const char* pnet_cr_type_str(pnet_cr_type_t type);

#endif /* PNET_PROTOCOL_H */
