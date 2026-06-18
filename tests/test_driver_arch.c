/**
 * test_driver_arch.c - Chapter 2: Driver Architecture Tests
 *
 * Tests for p-net driver architecture including:
 * - Protocol core layer (data encapsulation, transmission, reception)
 * - Device abstraction layer (device file operations)
 * - Memory mapping and interrupt handling
 * - Communication mechanisms (cyclic/acyclic, real-time)
 */

#include "test_framework.h"
#include "pnet_device.h"
#include "pnet_protocol.h"

/* ===== Device Layer Tests ===== */

void test_device_init(void) {
    TEST_BEGIN("device_init_basic");

    pnet_device_t dev;
    int ret = pnet_device_init(&dev);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(PNET_DEV_STATE_INITIALIZED, dev.state);
    ASSERT_EQ(-1, dev.fd);
    ASSERT_EQ(0, dev.error_count);

    pnet_device_cleanup(&dev);
    TEST_PASS();
}

void test_device_init_null(void) {
    TEST_BEGIN("device_init_null_safety");

    int ret = pnet_device_init(NULL);
    ASSERT_EQ(-1, ret);

    TEST_PASS();
}

void test_device_open_close(void) {
    TEST_BEGIN("device_open_close_lifecycle");

    pnet_device_t dev;
    pnet_device_init(&dev);

    /* Open with a non-existent path (simulation mode) */
    int ret = pnet_device_open(&dev, "/dev/pnet_test_nonexistent");
    ASSERT_EQ(0, ret);
    ASSERT_EQ(PNET_DEV_STATE_OPEN, dev.state);

    ret = pnet_device_close(&dev);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(PNET_DEV_STATE_CLOSED, dev.state);

    pnet_device_cleanup(&dev);
    TEST_PASS();
}

void test_device_read_write_simulation(void) {
    TEST_BEGIN("device_read_write_simulation");

    pnet_device_t dev;
    pnet_device_init(&dev);
    pnet_device_open(&dev, "/dev/pnet_test");

    /* Write data */
    const char *write_data = "Profinet RT Data";
    int written = pnet_device_write(&dev, write_data, strlen(write_data));
    ASSERT_GT(written, 0);
    ASSERT_EQ((int)strlen(write_data), written);

    /* Pre-load rx_buffer for read simulation */
    const char *rx_data = "Sensor Reading OK";
    dev.rx_len = strlen(rx_data);
    memcpy(dev.rx_buffer, rx_data, dev.rx_len);

    /* Read data */
    char read_buf[64] = {0};
    int bytes_read = pnet_device_read(&dev, read_buf, sizeof(read_buf));
    ASSERT_GT(bytes_read, 0);
    ASSERT_STR_EQ("Sensor Reading OK", read_buf);

    pnet_device_close(&dev);
    pnet_device_cleanup(&dev);
    TEST_PASS();
}

void test_device_info(void) {
    TEST_BEGIN("device_info_get_set");

    pnet_device_t dev;
    pnet_device_init(&dev);

    pnet_device_info_t info = {
        .vendor_id = 0x0001,
        .device_id = 0x0002,
        .profile_id = 0x0100,
        .type = PNET_DEVICE_TYPE_IO_DEVICE
    };
    strncpy(info.device_name, "Test IO Device", sizeof(info.device_name) - 1);
    strncpy(info.station_name, "STATION-001", sizeof(info.station_name) - 1);
    info.ip_addr = 0xC0A80064; /* 192.168.0.100 */

    int ret = pnet_device_set_info(&dev, &info);
    ASSERT_EQ(0, ret);

    pnet_device_info_t read_info;
    ret = pnet_device_get_info(&dev, &read_info);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(0x0001, (int)read_info.vendor_id);
    ASSERT_EQ(0x0002, (int)read_info.device_id);
    ASSERT_STR_EQ("Test IO Device", read_info.device_name);

    pnet_device_cleanup(&dev);
    TEST_PASS();
}

void test_device_state_str(void) {
    TEST_BEGIN("device_state_string_conversion");

    ASSERT_STR_EQ("UNINITIALIZED", pnet_device_state_str(PNET_DEV_STATE_UNINITIALIZED));
    ASSERT_STR_EQ("INITIALIZED", pnet_device_state_str(PNET_DEV_STATE_INITIALIZED));
    ASSERT_STR_EQ("OPEN", pnet_device_state_str(PNET_DEV_STATE_OPEN));
    ASSERT_STR_EQ("RUNNING", pnet_device_state_str(PNET_DEV_STATE_RUNNING));
    ASSERT_STR_EQ("ERROR", pnet_device_state_str(PNET_DEV_STATE_ERROR));
    ASSERT_STR_EQ("CLOSED", pnet_device_state_str(PNET_DEV_STATE_CLOSED));

    TEST_PASS();
}

/* ===== Memory Mapping Tests ===== */

void test_mmap_create_destroy(void) {
    TEST_BEGIN("memory_map_create_and_destroy");

    pnet_device_t dev;
    pnet_device_init(&dev);

    pnet_mmap_t mmap;
    int ret = pnet_mmap_create(&dev, &mmap, 0x10000000, 4096);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(mmap.mapped);
    ASSERT_NOT_NULL(mmap.virt_addr);
    ASSERT_EQ(4096, (int)mmap.size);

    /* Write to mapped memory */
    void *addr = pnet_mmap_get_addr(&mmap);
    ASSERT_NOT_NULL(addr);
    memset(addr, 0xAB, 4096);

    ret = pnet_mmap_destroy(&mmap);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(mmap.mapped);
    ASSERT_NULL(mmap.virt_addr);

    pnet_device_cleanup(&dev);
    TEST_PASS();
}

void test_mmap_null_safety(void) {
    TEST_BEGIN("memory_map_null_safety");

    int ret = pnet_mmap_create(NULL, NULL, 0, 0);
    ASSERT_EQ(-1, ret);

    ret = pnet_mmap_destroy(NULL);
    ASSERT_EQ(-1, ret);

    void *addr = pnet_mmap_get_addr(NULL);
    ASSERT_NULL(addr);

    TEST_PASS();
}

/* ===== Interrupt Handling Tests ===== */

static int g_irq_callback_count = 0;
static void test_irq_handler(void *context, uint32_t irq_number) {
    (void)irq_number;
    g_irq_callback_count++;
    if (context) {
        (*(int *)context)++;
    }
}

void test_irq_register_simulate(void) {
    TEST_BEGIN("irq_register_and_simulate");

    pnet_device_t dev;
    pnet_device_init(&dev);

    int context_value = 0;
    g_irq_callback_count = 0;

    pnet_irq_info_t irq = {
        .irq_number = 42,
        .handler = test_irq_handler,
        .context = &context_value,
        .registered = false,
        .irq_count = 0
    };

    int ret = pnet_irq_register(&dev, &irq);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(irq.registered);

    /* Simulate 5 interrupts */
    for (int i = 0; i < 5; i++) {
        ret = pnet_irq_simulate(&irq);
        ASSERT_EQ(0, ret);
    }
    ASSERT_EQ(5, g_irq_callback_count);
    ASSERT_EQ(5, context_value);
    ASSERT_EQ(5, (int)irq.irq_count);

    ret = pnet_irq_unregister(&irq);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(irq.registered);

    /* Simulate after unregister should fail */
    ret = pnet_irq_simulate(&irq);
    ASSERT_EQ(-1, ret);

    pnet_device_cleanup(&dev);
    TEST_PASS();
}

/* ===== Protocol Core Layer Tests ===== */

void test_protocol_init_shutdown(void) {
    TEST_BEGIN("protocol_init_and_shutdown");

    int ret = pnet_protocol_init();
    ASSERT_EQ(0, ret);

    /* Double init should fail */
    ret = pnet_protocol_init();
    ASSERT_EQ(-1, ret);

    pnet_protocol_shutdown();

    /* Can re-init after shutdown */
    ret = pnet_protocol_init();
    ASSERT_EQ(0, ret);
    pnet_protocol_shutdown();

    TEST_PASS();
}

void test_frame_build_parse(void) {
    TEST_BEGIN("frame_build_and_parse_roundtrip");

    pnet_protocol_init();

    pnet_io_data_t io_data;
    memset(&io_data, 0, sizeof(io_data));
    io_data.slot = 0;
    io_data.subslot = 1;
    io_data.input_data[0] = 0xDE;
    io_data.input_data[1] = 0xAD;
    io_data.input_len = 2;
    io_data.output_data[0] = 0xBE;
    io_data.output_data[1] = 0xEF;
    io_data.output_len = 2;

    /* Build frame */
    pnet_frame_t frame;
    int ret = pnet_frame_build(&frame, &io_data);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(frame.valid);
    ASSERT_EQ(4, (int)frame.payload_len);

    /* Serialize to raw bytes */
    uint8_t raw[PNET_MAX_FRAME_SIZE];
    size_t raw_len = pnet_frame_serialize(&frame, raw, sizeof(raw));
    ASSERT_GT((int)raw_len, 0);

    /* Parse back */
    pnet_frame_t parsed;
    ret = pnet_frame_parse(raw, raw_len, &parsed);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(parsed.valid);
    ASSERT_EQ(frame.payload_len, parsed.payload_len);

    /* Verify payload */
    ASSERT_EQ(0xDE, parsed.payload[0]);
    ASSERT_EQ(0xAD, parsed.payload[1]);
    ASSERT_EQ(0xBE, parsed.payload[2]);
    ASSERT_EQ(0xEF, parsed.payload[3]);

    pnet_protocol_shutdown();
    TEST_PASS();
}

void test_frame_validate_invalid(void) {
    TEST_BEGIN("frame_validate_invalid_frames");

    /* NULL frame */
    int ret = pnet_frame_validate(NULL);
    ASSERT_EQ(-1, ret);

    /* Invalid frame */
    pnet_frame_t bad_frame;
    memset(&bad_frame, 0, sizeof(bad_frame));
    bad_frame.valid = false;
    ret = pnet_frame_validate(&bad_frame);
    ASSERT_EQ(-1, ret);

    TEST_PASS();
}

/* ===== Communication Relationship Tests ===== */

void test_cr_lifecycle(void) {
    TEST_BEGIN("communication_relationship_lifecycle");

    pnet_cr_descriptor_t cr;
    int ret = pnet_cr_create(&cr, PNET_CR_TYPE_IO_DATA, PNET_RT_CLASS_RT);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(cr.active);
    ASSERT_EQ(PNET_CR_TYPE_IO_DATA, cr.type);
    ASSERT_EQ(PNET_RT_CLASS_RT, cr.rt_class);

    ret = pnet_cr_activate(&cr);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(cr.active);

    ret = pnet_cr_deactivate(&cr);
    ASSERT_EQ(0, ret);
    ASSERT_FALSE(cr.active);

    ret = pnet_cr_destroy(&cr);
    ASSERT_EQ(0, ret);

    TEST_PASS();
}

void test_cyclic_data_exchange(void) {
    TEST_BEGIN("cyclic_data_send_receive");

    pnet_protocol_init();

    pnet_cr_descriptor_t cr;
    pnet_cr_create(&cr, PNET_CR_TYPE_IO_DATA, PNET_RT_CLASS_RT);
    pnet_cr_activate(&cr);

    /* Prepare IO data for cyclic exchange */
    pnet_io_data_t send_data;
    memset(&send_data, 0, sizeof(send_data));
    send_data.slot = 0;
    send_data.subslot = 1;
    send_data.input_data[0] = 42;
    send_data.input_len = 1;
    send_data.output_len = 0;

    /* Send cyclic data */
    int ret = pnet_cyclic_send(&cr, &send_data);
    ASSERT_EQ(0, ret);

    /* Receive cyclic data */
    pnet_io_data_t recv_data;
    memset(&recv_data, 0, sizeof(recv_data));
    ret = pnet_cyclic_receive(&cr, &recv_data);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(recv_data.data_valid);

    /* Sending on inactive CR should fail */
    pnet_cr_deactivate(&cr);
    ret = pnet_cyclic_send(&cr, &send_data);
    ASSERT_EQ(-1, ret);

    pnet_cr_destroy(&cr);
    pnet_protocol_shutdown();
    TEST_PASS();
}

void test_acyclic_data_exchange(void) {
    TEST_BEGIN("acyclic_read_write");

    /* Acyclic read */
    char buffer[256];
    int ret = pnet_acyclic_read(0, 1, 100, buffer, sizeof(buffer));
    ASSERT_GT(ret, 0);

    /* Acyclic write */
    const char *data = "Config param value";
    ret = pnet_acyclic_write(0, 1, 100, data, strlen(data));
    ASSERT_EQ((int)strlen(data), ret);

    TEST_PASS();
}

void test_protocol_stats(void) {
    TEST_BEGIN("protocol_statistics_tracking");

    pnet_protocol_init();
    pnet_protocol_reset_stats();

    pnet_protocol_stats_t stats;
    pnet_protocol_get_stats(&stats);
    ASSERT_EQ(0, (int)stats.frames_sent);
    ASSERT_EQ(0, (int)stats.frames_received);

    /* Generate some traffic */
    pnet_cr_descriptor_t cr;
    pnet_cr_create(&cr, PNET_CR_TYPE_IO_DATA, PNET_RT_CLASS_RT);
    pnet_cr_activate(&cr);

    pnet_io_data_t data;
    memset(&data, 0, sizeof(data));
    data.input_data[0] = 0x01;
    data.input_len = 1;

    for (int i = 0; i < 10; i++) {
        pnet_cyclic_send(&cr, &data);
        pnet_cyclic_receive(&cr, &data);
    }

    pnet_protocol_get_stats(&stats);
    ASSERT_EQ(10, (int)stats.frames_sent);

    pnet_cr_destroy(&cr);
    pnet_protocol_shutdown();
    TEST_PASS();
}

void test_utility_functions(void) {
    TEST_BEGIN("protocol_utility_string_conversions");

    ASSERT_STR_EQ("NRT/UDP", pnet_rt_class_str(PNET_RT_CLASS_UDP));
    ASSERT_STR_EQ("RT", pnet_rt_class_str(PNET_RT_CLASS_RT));
    ASSERT_STR_EQ("IRT", pnet_rt_class_str(PNET_RT_CLASS_IRT));
    ASSERT_STR_EQ("TSN", pnet_rt_class_str(PNET_RT_CLASS_TSN));

    ASSERT_STR_EQ("IO_DATA", pnet_cr_type_str(PNET_CR_TYPE_IO_DATA));
    ASSERT_STR_EQ("ALARM", pnet_cr_type_str(PNET_CR_TYPE_ALARM));

    uint16_t fid = pnet_frame_id(0x80, 0x01);
    ASSERT_EQ(0x8001, (int)fid);

    TEST_PASS();
}

/* ===== Run all chapter 2 tests ===== */
void run_chapter2_tests(void) {
    TEST_SUITE_BEGIN("Chapter 2: Driver Architecture & Protocol Core");

    test_device_init();
    test_device_init_null();
    test_device_open_close();
    test_device_read_write_simulation();
    test_device_info();
    test_device_state_str();

    test_mmap_create_destroy();
    test_mmap_null_safety();

    test_irq_register_simulate();

    test_protocol_init_shutdown();
    test_frame_build_parse();
    test_frame_validate_invalid();

    test_cr_lifecycle();
    test_cyclic_data_exchange();
    test_acyclic_data_exchange();
    test_protocol_stats();
    test_utility_functions();

    TEST_SUITE_END();
}
