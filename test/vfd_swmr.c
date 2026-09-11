/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by Akadio, Inc.                                                 *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/***********************************************************
 *
 * Test program:
 *
 * Tests the VFD SWMR Feature.
 *
 * Note: Relevant tests in this file are modified to reflect the
 *       changes to the fapl for VDS.  See latest RFC.
 *
 *************************************************************/

#include "H5queue.h"
#include "h5test.h"
#include "vfd_swmr_common.h"

/*
 * This file needs to access private information from the H5F package.
 */

#define H5F_FRIEND  /*suppress error about including H5Fpkg */
#define H5FD_FRIEND /*suppress error about including H5FDpkg */
#define H5F_TESTING
#define H5FD_TESTING
#include "H5FDprivate.h"
#include "H5Fpkg.h"
#include "H5FDpkg.h"
#include "H5Iprivate.h"
#include "H5MVprivate.h" /* Free-space manager for the VFD SWMR metadata file */
#include "H5CXprivate.h" /* API Contexts                                    */
#define H5PB_FRIEND      /*suppress error about including H5PBpkg */
#include "H5PBpkg.h"     /* Page buffer, for H5PB_entry_t                   */
#include "H5ACprivate.h" /* Metadata cache, for the client class tables     */
#include "H5MFprivate.h" /* File memory management                          */
#include "H5PBprivate.h" /* Page buffer                                     */

#define H5FD_FRIEND /*suppress error about including H5FDpkg      */
#include "H5FDpkg.h"

#define FS_PAGE_SIZE 512

#define FILENAME  "vfd_swmr_file"
#define FILENAME2 "vfd_swmr_file2.h5"
#define FILENAME3 "vfd_swmr_file3.h5"
#define FILENAME4 "vfd_swmr_file4.h5"
#define FNAME     "non_vfd_swmr_file"

static const char *namebases[] = {FILENAME, FILENAME2, FILENAME3, FNAME, NULL};
#define namebase     FILENAME
#define namebase2    FILENAME2
#define namebase3    FILENAME3
#define namebase4    FILENAME4
#define non_namebase FNAME

/* FILENAME */
#define MD_FILENAME "vfd_swmr_metadata_file"
#define UD_FILENAME "vfd_swmr_updater_file"

/* FILENAME2 */
#define MD_FILENAME2 "vfd_swmr_metadata_file2"

/* FILENAME3 */
#define MD_FILENAME3 "vfd_swmr_metadata_file3"

/* FILENAME4 */
#define MD_FILE "vfd_swmr_md_file"
#define UD_FILE "vfd_swmr_ud_file"

#define FILE_NAME_LEN 1024

/* Defines used by verify_updater_flags() and verify_ud_chk() helper routine */

/* Offset of "flags" in updater file header */
#define UD_HD_FLAGS_OFFSET 6

/* Offset of "sequence number" in updater file header */
#define UD_HD_SEQ_NUM_OFFSET 12

/* Offset of "change list length" in updater file header */
#define UD_HD_CHANGE_LIST_LEN_OFFSET 36

/* Offset of "number of change list entries" in updater file change list header */
#define UD_CL_NUM_CHANGE_LIST_ENTRIES_OFFSET H5F_UD_HEADER_SIZE + 44

/* Size of "sequence number", "tick number" and "change list length" fields in the updater file header */
#define UD_SIZE_8 8

/* Size of checksum and "number of change list entries" field in the updater change list header */
#define UD_SIZE_4 4

/* Size of "flags" field in the updater file header */
#define UD_SIZE_2 2

/* Note that bitwise operations often perform integral promotion, so you have
 * to cast the result to avoid warnings.
 */
#define Swap2Bytes(val) (uint16_t)((((val) >> 8) & 0x00FF) | (((val) << 8) & 0xFF00))

#define Swap8Bytes(val)                                                                                      \
    ((((val) >> 56) & 0x00000000000000FF) | (((val) >> 40) & 0x000000000000FF00) |                           \
     (((val) >> 24) & 0x0000000000FF0000) | (((val) >> 8) & 0x00000000FF000000) |                            \
     (((val) << 8) & 0x000000FF00000000) | (((val) << 24) & 0x0000FF0000000000) |                            \
     (((val) << 40) & 0x00FF000000000000) | (((val) << 56) & 0xFF00000000000000))
#define Swap4Bytes(val)                                                                                      \
    ((((val) >> 24) & 0x000000FF) | (((val) >> 8) & 0x0000FF00) | (((val) << 8) & 0x00FF0000) |              \
     (((val) << 24) & 0xFF000000))

/* test routines for VFD SWMR */
static unsigned test_fapl(hid_t orig_fapl);
static unsigned test_file_fapl(hid_t orig_fapl);
static unsigned test_shadow_index_lookup(void);

static unsigned test_writer_md(hid_t orig_fapl);
static unsigned test_writer_create_open_flush(hid_t orig_fapl);

static unsigned test_enable_disable_eot(hid_t orig_fapl);
static unsigned test_enable_disable_eot_concur(hid_t orig_fapl);

static unsigned test_file_end_tick(hid_t orig_fapl);
static unsigned test_file_end_tick_concur(hid_t orig_fapl);

static unsigned test_same_file_opens(hid_t orig_fapl, hbool_t presume);
static unsigned test_vfds_same_file_opens(hid_t orig_fapl, const char *env_h5_drvr);

static unsigned test_multiple_file_opens(hid_t orig_fapl);
static unsigned test_multiple_file_opens_concur(hid_t orig_fapl);

static unsigned test_reader_md_concur(hid_t orig_fapl);

static unsigned test_make_believe_multiple_file_opens_concur(hid_t orig_fapl);
static unsigned test_auto_generate_md(hid_t orig_fapl, const char *md_path);
static unsigned test_long_md_path_name(hid_t orig_fapl);
static unsigned test_auto_long_md_path_name(hid_t orig_fapl);

static unsigned test_updater_flags(hid_t orig_fapl);
static unsigned test_updater_flags_same_file_opens(hid_t orig_fapl);
static herr_t   verify_updater_flags(char *ud_name, uint16_t expected_flags);

static unsigned test_updater_generate_md_checksums(hid_t orig_fapl, hbool_t file_create);
static void     clean_chk_ud_files(char *md_file_path, char *updater_file_path);
static herr_t   verify_ud_chk(char *md_file_path, char *ud_file_path);
static herr_t   md_ck_cb(char *md_file_path, uint64_t tick_num);

void       check_endian(hbool_t *little_endian);
static int vfd_swmr_fapl_augment(hid_t fapl, bool use_latest_format, bool only_meta_pages,
                                 size_t page_buf_size, H5F_vfd_swmr_config_t *config);

/*-------------------------------------------------------------------------
 *
 * Function     check_endian()
 *              Helper routine to check the endianness of a machine
 *
 * -------------------------------------------------------------------------
 */
void
check_endian(hbool_t *little_endian)
{
    short int word = 0x0001;
    char     *byte = (char *)&word;

    if (byte[0] == 1)
        /* little endian */
        *little_endian = true;
    else
        /* big endian */
        *little_endian = false;

} /* check_endian() */

/*-------------------------------------------------------------------------
 *
 * Function     vfd_swmr_fapl_augment()
 *              Helper routine to set up fapl for VFD SWMR
 *
 *-------------------------------------------------------------------------
 */
static int
vfd_swmr_fapl_augment(hid_t fapl, bool use_latest_format, bool only_meta_pages, size_t page_buf_size,
                      H5F_vfd_swmr_config_t *config)
{
    if (use_latest_format) {
        if (H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST) < 0)
            return -1;
    }
    else { /* Currently this is used only for old-styled group implementation tests.*/
        if (H5Pset_libver_bounds(fapl, H5F_LIBVER_EARLIEST, H5F_LIBVER_LATEST) < 0)
            return -1;
    }

    /* Enable page buffering */
    if (H5Pset_page_buffer_size(fapl, page_buf_size, only_meta_pages ? 100 : 0, 0) < 0)
        return -1;

    /* Enable VFD SWMR configuration */
    if (H5Pset_vfd_swmr_config(fapl, config) < 0)
        return -1;

    return 0;
} /* vfd_swmr_fapl_augment() */

/*-------------------------------------------------------------------------
 * Function:    test_fapl()
 *
 * Purpose:     A) Verify that invalid info set in the fapl fails
 *                 as expected (see the RFC for VFD SWMR):
 *                 --version: should be a known version
 *                 --tick_len: should be >= 0
 *                 --max_lag: should be >= 3
 *                 --md_pages_reserved: should be >= 2
 *                 --at least one of maintain_metadata_file and generate_updater_files
 *                   must be true
 *                 --if both the writer and generate_updater_files fields are true,
 *                   then updater_file_path field shouldn't be empty
 *              B) Verify that info set in the fapl is retrieved correctly.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; July 2018
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_fapl(hid_t orig_fapl)
{
    hid_t                  fapl      = H5I_INVALID_HID; /* File access property list */
    H5F_vfd_swmr_config_t *my_config = NULL;            /* Configuration for VFD SWMR */
    herr_t                 ret;                         /* Return value */

    TESTING("Configure VFD SWMR with fapl");

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* Allocate memory for the configuration structure */
    if ((my_config = malloc(sizeof(*my_config))) == NULL)
        FAIL_STACK_ERROR;
    memset(my_config, 0, sizeof(*my_config));

    /* Get a copy of the file access property list */
    if ((fapl = H5Pcreate(H5P_FILE_ACCESS)) < 0)
        TEST_ERROR;

    /* Should get invalid VFD SWMR config info */
    if (H5Pget_vfd_swmr_config(fapl, my_config) < 0)
        TEST_ERROR;

    /* Verify that the version is incorrect */
    if (my_config->version >= H5F__CURR_VFD_SWMR_CONFIG_VERSION)
        TEST_ERROR;

    /* Should fail: version is 0 */
    H5E_BEGIN_TRY
    {
        ret = H5Pset_vfd_swmr_config(fapl, my_config);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Set valid version */
    my_config->version = H5F__CURR_VFD_SWMR_CONFIG_VERSION;

    /* Set valid tick_len */
    my_config->tick_len = 3;
    /* Should fail: max_lag is 2 */
    my_config->max_lag = 2;
    H5E_BEGIN_TRY
    {
        ret = H5Pset_vfd_swmr_config(fapl, my_config);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Set valid max_lag */
    my_config->max_lag = 3;
    /* Should fail: md_pages_reserved is 0 */
    H5E_BEGIN_TRY
    {
        ret = H5Pset_vfd_swmr_config(fapl, my_config);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Set valid md_pages_reserved */
    my_config->md_pages_reserved = 2;
    my_config->writer            = true;

    /* Should fail: at least one of maintain_metadata_file and generate_updater_files must be true */
    H5E_BEGIN_TRY
    {
        ret = H5Pset_vfd_swmr_config(fapl, my_config);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    my_config->writer                 = true;
    my_config->maintain_metadata_file = true;
    my_config->generate_updater_files = true;

    /* Should fail: empty updater_file_path */
    H5E_BEGIN_TRY
    {
        ret = H5Pset_vfd_swmr_config(fapl, my_config);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Set md_file_name */
    strcpy(my_config->md_file_name, MD_FILENAME);
    my_config->generate_updater_files = false;

    /* Should succeed in setting the configuration info */
    if (H5Pset_vfd_swmr_config(fapl, my_config) < 0)
        TEST_ERROR;

    /* Clear the configuration structure */
    memset(my_config, 0, sizeof(H5F_vfd_swmr_config_t));

    /* Retrieve the configuration info just set */
    if (H5Pget_vfd_swmr_config(fapl, my_config) < 0)
        TEST_ERROR;

    /* Verify the configuration info */
    if (my_config->version < H5F__CURR_VFD_SWMR_CONFIG_VERSION)
        TEST_ERROR;
    if (my_config->md_pages_reserved != 2)
        TEST_ERROR;
    if (my_config->generate_updater_files)
        TEST_ERROR;

    /* Check md_file_name instead of md_file_path */
    if (strcmp(my_config->md_file_name, MD_FILENAME) != 0)
        TEST_ERROR;

    my_config->generate_updater_files = true;
    /* Set updater_file_path */
    strcpy(my_config->updater_file_path, UD_FILENAME);

    /* Should succeed in setting the configuration info */
    if (H5Pset_vfd_swmr_config(fapl, my_config) < 0)
        TEST_ERROR;

    /* Clear the configuration structure */
    memset(my_config, 0, sizeof(H5F_vfd_swmr_config_t));

    /* Retrieve the configuration info just set */
    if (H5Pget_vfd_swmr_config(fapl, my_config) < 0)
        TEST_ERROR;

    /* Verify the configuration info */
    if (!my_config->generate_updater_files)
        TEST_ERROR;
    if (strcmp(my_config->updater_file_path, UD_FILENAME) != 0)
        TEST_ERROR;
    if (!my_config->maintain_metadata_file)
        TEST_ERROR;

    /* Check md_file_name instead of md_file_path */
    if (strcmp(my_config->md_file_name, MD_FILENAME) != 0)
        TEST_ERROR;

    /* Close the file access property list */
    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;

    free(my_config);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
    }
    H5E_END_TRY;

    free(my_config);

    return 1;
} /* test_fapl() */

/*-------------------------------------------------------------------------
 * Function:    test_file_fapl()
 *
 * Purpose:     A) Verify that page buffering and paged aggregation
 *                 have to be enabled for a file to be configured
 *                 with VFD SWMR.
 *              B) Verify that the "writer" setting in the fapl's VFD
 *                 SWMR configuration should be consistent with the
 *                 file access flags.
 *              C) Verify the VFD SWMR configuration set in fapl
 *                 used to create/open the file is the same as the
 *                 configuration retrieved from the file's fapl.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; July 2018
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_file_fapl(hid_t orig_fapl)
{
    char  filename[FILE_NAME_LEN];             /* Filename to use */
    hid_t fid       = H5I_INVALID_HID;         /* File ID */
    hid_t fid2      = H5I_INVALID_HID;         /* File ID */
    hid_t fcpl      = H5I_INVALID_HID;         /* File creation property list ID */
    hid_t fapl1     = H5I_INVALID_HID;         /* File access property list ID associated with the file */
    hid_t fapl2     = H5I_INVALID_HID;         /* File access property list ID associated with the file */
    hid_t file_fapl = H5I_INVALID_HID;         /* File access property list ID associated with the file */
    H5F_vfd_swmr_config_t *config1     = NULL; /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *config2     = NULL; /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *file_config = NULL; /* Configuration for VFD SWMR */

    TESTING("VFD SWMR configuration for the file and fapl");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Should succeed without VFD SWMR configured */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Close the file  */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Allocate memory for the configuration structure */
    if ((config1 = (H5F_vfd_swmr_config_t *)malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;
    if ((config2 = (H5F_vfd_swmr_config_t *)malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;
    if ((file_config = (H5F_vfd_swmr_config_t *)malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Configured as VFD SWMR reader + no page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 7, false, false, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 0, config1) < 0)
        FAIL_STACK_ERROR;

    /* Should fail to create: file access is writer but VFD SWMR config is reader */
    H5E_BEGIN_TRY
    {
        fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl1);
    }
    H5E_END_TRY;
    if (fid >= 0)
        TEST_ERROR;

    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;

    /*
     * Configured as VFD SWMR writer + no page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 7, false, true, true, true, true, 2, NULL, MD_FILENAME, UD_FILENAME);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 0, config1) < 0)
        FAIL_STACK_ERROR;

    /* Should fail to create: page buffering and paged aggregation not enabled */
    H5E_BEGIN_TRY
    {
        fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl1);
    }
    H5E_END_TRY;
    if (fid >= 0)
        TEST_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Should fail to create: no page buffering */
    H5E_BEGIN_TRY
    {
        fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl1);
    }
    H5E_END_TRY;
    if (fid >= 0)
        TEST_ERROR;

    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;

    /*
     * Configured as VFD SWMR writer + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 7, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* Should succeed to create the file: paged aggregation and page buffering enabled */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl1)) < 0)
        TEST_ERROR;

    /* Get the file's file access property list */
    if ((file_fapl = H5Fget_access_plist(fid)) < 0)
        FAIL_STACK_ERROR;

    /* Retrieve the VFD SWMR configuration from file_fapl */
    if (H5Pget_vfd_swmr_config(file_fapl, file_config) < 0)
        TEST_ERROR;

    /* Verify the retrieved info is the same as config1 */
    if (memcmp(config1, file_config, sizeof(H5F_vfd_swmr_config_t)) != 0)
        TEST_ERROR;

    /* Closing */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(file_fapl) < 0)
        FAIL_STACK_ERROR;

    /* Should fail to open: file access is reader but VFD SWMR config is writer */
    H5E_BEGIN_TRY
    {
        fid = H5Fopen(filename, H5F_ACC_RDONLY, fapl1);
    }
    H5E_END_TRY;
    if (fid >= 0)
        TEST_ERROR;

    /* Should succeed to open: file access and VFD SWMR config are consistent */
    if ((fid = H5Fopen(filename, H5F_ACC_RDWR, fapl1)) < 0)
        TEST_ERROR;

    /* Get the file's file access property list */
    if ((file_fapl = H5Fget_access_plist(fid)) < 0)
        FAIL_STACK_ERROR;

    /* Clear info in file_config */
    memset(file_config, 0, sizeof(H5F_vfd_swmr_config_t));

    /* Retrieve the VFD SWMR configuration from file_fapl */
    if (H5Pget_vfd_swmr_config(file_fapl, file_config) < 0)
        TEST_ERROR;

    /* Verify the retrieved info is the same as config1 */
    if (memcmp(config1, file_config, sizeof(H5F_vfd_swmr_config_t)) != 0)
        TEST_ERROR;

    /* Closing */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(file_fapl) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;

    /*
     * Set up different VFD SWMR configuration + page_buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config2, 4, 10, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl2 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl2, false, false, 4096, config2) < 0)
        FAIL_STACK_ERROR;

    /* Should succeed to open the file as VFD SWMR writer */
    if ((fid = H5Fopen(filename, H5F_ACC_RDWR, fapl2)) < 0)
        TEST_ERROR;

    /* Get the file's file access property list */
    if ((file_fapl = H5Fget_access_plist(fid)) < 0)
        FAIL_STACK_ERROR;

    /* Clear info in file_config */
    memset(file_config, 0, sizeof(H5F_vfd_swmr_config_t));

    /* Retrieve the VFD SWMR configuration from file_fapl */
    if (H5Pget_vfd_swmr_config(file_fapl, file_config) < 0)
        TEST_ERROR;

    /* Verify the retrieved info is NOT the same as config1 */
    if (memcmp(config1, file_config, sizeof(H5F_vfd_swmr_config_t)) == 0)
        TEST_ERROR;

    /* Verify the retrieved info is the same as config2 */
    if (memcmp(config2, file_config, sizeof(H5F_vfd_swmr_config_t)) != 0)
        TEST_ERROR;

    /* The file previously opened as VDF SWMR writer is still open */
    /* with VFD SWMR configuration in config2 */

    /*
     * Set up as VFD SWMR writer in config1 but different from config2
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 3, 8, false, true, true, false, true, 3, NULL, MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* Re-open the same file with config1 */
    /* Should fail to open since config1 is different from config2 setting */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl1);
    }
    H5E_END_TRY;
    if (fid2 >= 0)
        TEST_ERROR;

    /* Close fapl1 */
    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;

    /*
     * Set up as VFD SWMR reader in config1 which is same as config2
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 10, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* Re-open the same file as VFD SWMR writer */
    /* Should succeed since config1 is same as the setting in config2 */
    if ((fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl1)) < 0)
        TEST_ERROR;

    /* Close fapl1 */
    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;

    memset(file_config, 0, sizeof(H5F_vfd_swmr_config_t));

    /* Get the file's file access property list */
    if ((file_fapl = H5Fget_access_plist(fid)) < 0)
        FAIL_STACK_ERROR;

    /* Retrieve the VFD SWMR configuration from file_fapl */
    if (H5Pget_vfd_swmr_config(file_fapl, file_config) < 0)
        TEST_ERROR;

    /* Should be the same as config1 */
    if (memcmp(config1, file_config, sizeof(H5F_vfd_swmr_config_t)) != 0)
        TEST_ERROR;

    /* Should be the the same as config2 */
    if (memcmp(config2, file_config, sizeof(H5F_vfd_swmr_config_t)) != 0)
        TEST_ERROR;

    /* Closing */
    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config1);
    free(config2);
    free(file_config);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(file_fapl);
        H5Pclose(fcpl);
        H5Pclose(fapl1);
        H5Pclose(fapl2);
        H5Fclose(fid);
        H5Fclose(fid2);
    }
    H5E_END_TRY;

    free(config1);
    free(config2);
    free(file_config);

    return 1;
} /* test_file_fapl() */

/*-------------------------------------------------------------------------
 * Function:    test_file_end_tick()
 *
 * Purpose:     Verify the public routine H5Fvfd_swmr_end_tick() works
 *              as described in the RFC for VFD SWMR.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; June 2020
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_file_end_tick(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN];   /* Filename to use */
    char                   filename2[FILE_NAME_LEN];  /* Filename to use */
    char                   filename3[FILE_NAME_LEN];  /* Filename to use */
    hid_t                  fid1    = H5I_INVALID_HID; /* File ID */
    hid_t                  fid2    = H5I_INVALID_HID; /* File ID */
    hid_t                  fid3    = H5I_INVALID_HID; /* File ID */
    hid_t                  fcpl    = H5I_INVALID_HID; /* File creation property list ID */
    hid_t                  fapl1   = H5I_INVALID_HID; /* File access property list ID */
    hid_t                  fapl2   = H5I_INVALID_HID; /* File access property list ID */
    hid_t                  fapl3   = H5I_INVALID_HID; /* File access property list ID */
    H5F_vfd_swmr_config_t *config1 = NULL;            /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *config2 = NULL;            /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *config3 = NULL;            /* Configuration for VFD SWMR */
    H5F_t                 *f1, *f2, *f3;              /* File pointer */
    uint64_t               s1 = 0;                    /* Saved tick_num */
    uint64_t               s2 = 0;                    /* Saved tick_num */
    uint64_t               s3 = 0;                    /* Saved tick_num */
    int                    ret;                       /* Return status */

    TESTING("H5Fvfd_swmr_end_tick()");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));
    h5_fixname(namebase2, orig_fapl, filename2, sizeof(filename2));
    h5_fixname(namebase3, orig_fapl, filename3, sizeof(filename3));

    /* Create a file without VFD SWMR configured */
    if ((fid1 = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Should fail to trigger EOT */
    H5E_BEGIN_TRY
    {
        ret = H5Fvfd_swmr_end_tick(fid1);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Close the file  */
    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;

    /* Allocate memory for the configuration structure */
    if ((config1 = malloc(sizeof(*config1))) == NULL)
        FAIL_STACK_ERROR;
    if ((config2 = malloc(sizeof(*config2))) == NULL)
        FAIL_STACK_ERROR;
    if ((config3 = malloc(sizeof(*config3))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Configured file 1 as VFD SWMR writer + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 10, 15, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /*
     * Configured file 2 as VFD SWMR writer + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config2, 5, 6, false, true, true, false, true, 2, NULL, MD_FILENAME2, NULL);

    if ((fapl2 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl2, false, false, 4096, config2) < 0)
        FAIL_STACK_ERROR;

    /*
     * Configured file 3 as VFD SWMR writer + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config3, 3, 6, false, true, true, false, true, 2, NULL, MD_FILENAME3, NULL);

    if ((fapl3 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl3, false, false, 4096, config3) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create file 1 with VFD SWMR writer */
    if ((fid1 = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl1)) < 0)
        TEST_ERROR;
    /* Keep file 1 opened */

    /* Create file 2 with VFD SWMR writer */
    if ((fid2 = H5Fcreate(filename2, H5F_ACC_TRUNC, fcpl, fapl2)) < 0)
        TEST_ERROR;
    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /* Create file 3 with VFD SWMR writer */
    if ((fid3 = H5Fcreate(filename3, H5F_ACC_TRUNC, fcpl, fapl3)) < 0)
        TEST_ERROR;
    if (H5Fclose(fid3) < 0)
        FAIL_STACK_ERROR;

    /* Open file 2 as VFD SWMR writer */
    if ((fid2 = H5Fopen(filename2, H5F_ACC_RDWR, fapl2)) < 0)
        TEST_ERROR;

    /* Open file 3 as VFD SWMR writer */
    if ((fid3 = H5Fopen(filename3, H5F_ACC_RDWR, fapl3)) < 0)
        TEST_ERROR;

    /* Get file pointer for the 3 files */
    f1 = H5VL_object(fid1);
    f2 = H5VL_object(fid2);
    f3 = H5VL_object(fid3);

    /* Saved tick_num for the 3 files */
    s1 = f1->shared->tick_num;
    s2 = f2->shared->tick_num;
    s3 = f3->shared->tick_num;

    /* Trigger EOT for file 2 */
    if (H5Fvfd_swmr_end_tick(fid2) < 0)
        TEST_ERROR;

    /* file 2: tick_num should increase or at least same as previous tick_num */
    if (f2->shared->tick_num < s2)
        TEST_ERROR;

    /* Disable EOT for file 2 */
    if (H5Fvfd_swmr_disable_end_of_tick(fid2) < 0)
        TEST_ERROR;

    /* Should fail to trigger end of tick processing for file 2 */
    H5E_BEGIN_TRY
    {
        ret = H5Fvfd_swmr_end_tick(fid2);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /* Trigger EOT for file 1 */
    if (H5Fvfd_swmr_end_tick(fid1) < 0)
        TEST_ERROR;

    /* file 1: tick_num should increase or at least same as previous tick_num */
    if (f1->shared->tick_num < s1)
        TEST_ERROR;

    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;

    /* Trigger EOT for file 3 */
    if (H5Fvfd_swmr_end_tick(fid3) < 0)
        TEST_ERROR;

    /* file 3: tick_num should increase or at least same as previous tick_num */
    if (f3->shared->tick_num < s3)
        TEST_ERROR;

    if (H5Fclose(fid3) < 0)
        FAIL_STACK_ERROR;

    /* Closing */
    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl2) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl3) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config1);
    free(config2);
    free(config3);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl1);
        H5Pclose(fapl2);
        H5Pclose(fapl3);
        H5Pclose(fcpl);
        H5Fclose(fid1);
        H5Fclose(fid2);
        H5Fclose(fid3);
    }
    H5E_END_TRY;

    free(config1);
    free(config2);
    free(config3);

    return 1;
} /* test_file_end_tick() */

/*-------------------------------------------------------------------------
 * Function:    test_writer_create_open_flush()
 *
 * Purpose:     Verify info in the metadata file when:
 *              --creating the HDF5 file
 *              --flushing the HDF5 file
 *              --opening an existing HDF5 file
 *              It will call the internal testing routine
 *              H5F__vfd_swmr_writer_create_open_flush_test() to do the following:
 *              --Open the metadata file
 *              --Verify the file size is as expected (md_pages_reserved)
 *              --For file create:
 *                  --No header magic is found
 *              --For file open or file flush:
 *                  --Read and decode the header and index in the metadata file
 *                  --Verify info in the header and index read from
 *                    the metadata file is as expected (empty index)
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2018
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_writer_create_open_flush(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN]; /* Filename to use */
    hid_t                  fid       = -1;          /* File ID */
    hid_t                  fapl      = -1;          /* File access property list */
    hid_t                  fcpl      = -1;          /* File creation property list */
    H5F_vfd_swmr_config_t *my_config = NULL;        /* Configuration for VFD SWMR */

    TESTING("Create/Open/Flush an HDF5 file for VFD SWMR");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Allocate memory for the configuration structure */
    if ((my_config = malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Set up the VFD SWMR configuration + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(my_config, 5, 10, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, 4096, my_config) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create an HDF5 file with VFD SWMR configured */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl)) < 0)
        FAIL_STACK_ERROR;

    /* Verify info in metadata file when creating the HDF5 file */
    if (H5F__vfd_swmr_writer_create_open_flush_test(fid, true) < 0)
        FAIL_STACK_ERROR;

    /* Flush the HDF5 file */
    if (H5Fflush(fid, H5F_SCOPE_GLOBAL) < 0)
        FAIL_STACK_ERROR;

    /* Verify info in metadata file when flushing the HDF5 file */
    if (H5F__vfd_swmr_writer_create_open_flush_test(fid, false) < 0)
        FAIL_STACK_ERROR;

    /* Close the file */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Re-open the file as VFD SWMR writer */
    if ((fid = H5Fopen(filename, H5F_ACC_RDWR, fapl)) < 0)
        FAIL_STACK_ERROR;

    /* Verify info in metadata file when reopening the HDF5 file */
    if (H5F__vfd_swmr_writer_create_open_flush_test(fid, false) < 0)
        FAIL_STACK_ERROR;

    /* Closing */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    free(my_config);

    PASSED();
    return 0;

error:

    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
        H5Pclose(fcpl);
        H5Fclose(fid);
    }
    H5E_END_TRY;

    free(my_config);

    return 1;
} /* test_writer_create_open_flush() */

/*-------------------------------------------------------------------------
 * Function:    test_writer_md()
 *
 * Purpose:     Verify info in the metadata file after updating with the
 *              constructed index: (A), (B), (C), (D)
 *              It will call the internal testing routine
 *              H5F__vfd_swmr_writer_md_test() to do the following:
 *              --Update the metadata file with the input index via the
 *                internal library routine H5F_update_vfd_swmr_metadata_file()
 *              --Verify the entries in the delayed list is as expected:
 *                --num_dl_entries
 *              --Open the metadata file, read and decode the header and index
 *              --Verify header and index info just read from the metadata
 *                file is as expected:
 *                --num_entries and index
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2018
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_writer_md(hid_t orig_fapl)
{
    char           filename[FILE_NAME_LEN];                            /* Filename to use */
    hid_t          fid         = H5I_INVALID_HID;                      /* File ID */
    hid_t          fapl        = H5I_INVALID_HID;                      /* File access property list */
    hid_t          fcpl        = H5I_INVALID_HID;                      /* File creation property list */
    const unsigned num_entries = 10;                                   /* index size */
    unsigned       i           = 0;                                    /* Local index variables */
    uint8_t       *buf         = NULL;                                 /* Data page from the page buffer */
    hid_t          dcpl        = H5I_INVALID_HID;                      /* Dataset creation property list */
    hid_t          sid         = H5I_INVALID_HID;                      /* Dataspace ID */
    hid_t          did         = H5I_INVALID_HID;                      /* Dataset ID */
    int           *rwbuf       = NULL;                                 /* Data buffer for writing */
    H5O_info2_t    oinfo;                                              /* Object metadata information */
    char           dname[100];                                         /* Name of dataset */
    hsize_t        dims[2]           = {50, 20};                       /* Dataset dimension sizes */
    hsize_t        max_dims[2]       = {H5S_UNLIMITED, H5S_UNLIMITED}; /* Dataset maximum dimension sizes */
    hsize_t        chunk_dims[2]     = {2, 5};                         /* Dataset chunked dimension sizes */
    H5FD_vfd_swmr_idx_entry_t *index = NULL;                           /* Pointer to the index entries */
    H5F_vfd_swmr_config_t     *my_config = NULL;                       /* Configuration for VFD SWMR */
    H5F_t                     *f         = NULL;                       /* Internal file object pointer */

    TESTING("Verify the metadata file for VFD SWMR writer");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Allocate memory for the configuration structure */
    if ((my_config = malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(my_config, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, FS_PAGE_SIZE, my_config) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, FS_PAGE_SIZE)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create an HDF5 file with VFD SWMR configured */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl)) < 0)
        FAIL_STACK_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f = (H5F_t *)H5VL_object(fid)))
        FAIL_STACK_ERROR;

    /* Allocate num_entries for the data buffer */
    if ((buf = calloc(num_entries, FS_PAGE_SIZE)) == NULL)
        FAIL_STACK_ERROR;

    /* Allocate memory for num_entries index */
    index = calloc(num_entries, sizeof(H5FD_vfd_swmr_idx_entry_t));
    if (NULL == index)
        FAIL_STACK_ERROR;

    /* (A) Construct index for updating the metadata file */
    for (i = 0; i < num_entries; i++) {
        index[i].hdf5_page_offset    = 3 + 7 * i;
        index[i].md_file_page_offset = 1 + (num_entries - i) * 5;
        index[i].length              = (uint32_t)FS_PAGE_SIZE;
        index[i].entry_ptr           = &buf[i * FS_PAGE_SIZE];
        index[i].tick_of_last_change = f->shared->tick_num;
    }

    /* Update with index and verify info in the metadata file */
    /* Also verify that 0 entries will be on the delayed list */
    if (H5F__vfd_swmr_writer_md_test(fid, num_entries, index, 0) < 0)
        TEST_ERROR;

    /* Create dataset creation property list */
    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        FAIL_STACK_ERROR;

    /* Set to use chunked dataset */
    if (H5Pset_chunk(dcpl, 2, chunk_dims) < 0)
        FAIL_STACK_ERROR;

    /* Create dataspace */
    if ((sid = H5Screate_simple(2, dims, max_dims)) < 0)
        FAIL_STACK_ERROR;

    /* Perform activities to ensure that max_lag ticks elapse */
    for (i = 0; i < my_config->max_lag + 1; i++) {
        decisleep(my_config->tick_len);

        /* Create a chunked dataset */
        sprintf(dname, "dset %d", i);
        if ((did = H5Dcreate2(fid, dname, H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            FAIL_STACK_ERROR;

        /* Get dataset object header address */
        if (H5Oget_info3(did, &oinfo, H5O_INFO_BASIC) < 0)
            FAIL_STACK_ERROR;

        /* Close the dataset */
        if (H5Dclose(did) < 0)
            FAIL_STACK_ERROR;
    }

    /* (B) Update every other entry in the index */
    for (i = 0; i < num_entries; i += 2) {
        index[i].entry_ptr           = &buf[i * FS_PAGE_SIZE];
        index[i].tick_of_last_change = f->shared->tick_num;
    }

    /* Update with index and verify info in the metadata file */
    /* Also verify that 5 entries will be on the delayed list */
    if (H5F__vfd_swmr_writer_md_test(fid, num_entries, index, 5) < 0)
        TEST_ERROR;

    /* Allocate memory for the read/write buffer */
    if ((rwbuf = malloc(sizeof(*rwbuf) * (50 * 20))) == NULL)
        FAIL_STACK_ERROR;
    for (i = 0; i < (50 * 20); i++)
        rwbuf[i] = (int)i;

    /* Perform activities to ensure that max_lag ticks elapse */
    for (i = 0; i < my_config->max_lag + 1; i++) {
        decisleep(my_config->tick_len);

        /* Open the dataset */
        sprintf(dname, "dset %d", i);
        if ((did = H5Dopen2(fid, dname, H5P_DEFAULT)) < 0)
            FAIL_STACK_ERROR;

        /* Write to the dataset */
        if (H5Dwrite(did, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, rwbuf) < 0)
            FAIL_STACK_ERROR;

        /* Get dataset object info */
        if (H5Oget_info3(did, &oinfo, H5O_INFO_BASIC) < 0)
            FAIL_STACK_ERROR;

        /* Close the dataset */
        if (H5Dclose(did) < 0)
            FAIL_STACK_ERROR;
    }

    /* (C) Update every 3 entry in the index */
    for (i = 0; i < num_entries; i += 3) {
        index[i].entry_ptr           = &buf[i * FS_PAGE_SIZE];
        index[i].tick_of_last_change = f->shared->tick_num;
    }

    /* Update with index and verify info in the metadata file */
    /* Also verify that 4 entries will be on the delayed list */
    if (H5F__vfd_swmr_writer_md_test(fid, num_entries, index, 4) < 0)
        TEST_ERROR;

    /* Clear the read/write buffer */
    memset(rwbuf, 0, sizeof(sizeof(int) * (50 * 20)));

    /* Perform activities to ensure that max_lag ticks elapse */
    for (i = 0; i < my_config->max_lag + 1; i++) {
        decisleep(my_config->tick_len);

        /* Open the dataset */
        sprintf(dname, "dset %d", i);
        if ((did = H5Dopen2(fid, dname, H5P_DEFAULT)) < 0)
            FAIL_STACK_ERROR;

        /* Read from the dataset */
        if (H5Dread(did, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, rwbuf) < 0)
            FAIL_STACK_ERROR;

        /* Get dataset object info */
        if (H5Oget_info3(did, &oinfo, H5O_INFO_BASIC) < 0)
            FAIL_STACK_ERROR;

        /* Close the dataset */
        if (H5Dclose(did) < 0)
            FAIL_STACK_ERROR;
    }

    /* (D) Update two entries in the index */
    index[1].entry_ptr           = &buf[1 * FS_PAGE_SIZE];
    index[1].tick_of_last_change = f->shared->tick_num;
    index[5].entry_ptr           = &buf[5 * FS_PAGE_SIZE];
    index[5].tick_of_last_change = f->shared->tick_num;

    /* Update with index and verify info in the metadata file */
    /* Also verify that 2 entries will be on the delayed list */
    if (H5F__vfd_swmr_writer_md_test(fid, num_entries, index, 2) < 0)
        TEST_ERROR;

    /* Closing */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;
    if (H5Sclose(sid) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(dcpl) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free resources */
    free(my_config);
    free(buf);
    free(rwbuf);
    free(index);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Dclose(did);
        H5Sclose(sid);
        H5Pclose(dcpl);
        H5Pclose(fapl);
        H5Pclose(fcpl);
        H5Fclose(fid);
    }
    H5E_END_TRY;

    free(my_config);
    free(buf);
    free(rwbuf);
    free(index);

    return 1;
} /* test_writer__md() */

#ifndef H5_HAVE_UNISTD_H

static unsigned
test_reader_md_concur(hid_t orig_fapl)
{
    /* Output message about test being performed */
    TESTING("Verify the metadata file for VFD SWMR reader");
    SKIPPED();
    puts("    Test skipped (unistd.h not present)");
    return 0;

} /* test_reader_md_concur() */

static unsigned
test_multiple_file_opens_concur(hid_t orig_fapl)
{
    /* Output message about test being performed */
    TESTING("EOT queue entries when opening files concurrently with VFD SWMR");
    SKIPPED();
    puts("    Test skipped (unistd.h not present)");
    return 0;

} /* test_multiple_file_opens_concur() */

static unsigned
test_enable_disable_eot_concur(hid_t orig_fapl)
{
    /* Output message about test being performed */
    TESTING("Verify concurrent H5Fvfd_swmr_enable/disable_end_of_tick()");
    SKIPPED();
    puts("    Test skipped (unistd.h not present)");
    return 0;

} /* test_enable_disable_eot_concur() */

static unsigned
test_file_end_tick_concur(hid_t orig_fapl)
{
    /* Output message about test being performed */
    TESTING("Verify concurrent H5Fvfd_swmr_end_tick()");
    SKIPPED();
    puts("    Test skipped (unistd.h not present)");
    return 0;

} /* test_file_end_tick_concur() */

static unsigned
test_make_believe_multiple_file_opens_concur(hid_t orig_fapl)
{
    /* Output message about test being performed */
    TESTING("Opening files concurrently as VFD SWMR reader and then as VFD SWMR writer");
    SKIPPED();
    puts("    Test skipped (unistd.h not present)");
    return 0;

} /* test_make_believe_multiple_file_opens_concur() */

#else /* H5_HAVE_UNISTD_H */

/*-------------------------------------------------------------------------
 * Function:    test_reader_md_concur()
 *
 * Purpose:     Verify metadata file info updated by the writer is
 *              what the reader obtained from the metadata file:
 *              --Cases (A), (B), (C), (D), (E)
 *              NOTE: Changes for page buffering/cache are not in place yet.
 *                    Index entries are constructed at the front end by the
 *                    writer and verified at the back end by the reader.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2018
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_reader_md_concur(hid_t orig_fapl)
{
    char        filename[FILE_NAME_LEN]; /* Filename to use */
    unsigned    i     = 0;               /* Local index variables */
    uint8_t    *buf   = NULL;            /* Data page from the page buffer */
    hid_t       dcpl  = H5I_INVALID_HID; /* Dataset creation property list */
    hid_t       sid   = H5I_INVALID_HID; /* Dataspace ID */
    hid_t       did   = H5I_INVALID_HID; /* Dataset ID */
    int        *rwbuf = NULL;            /* Data buffer for writing */
    H5O_info2_t oinfo;                   /* Object metadata information */
    char        dname[100];              /* Name of dataset */
    hsize_t     dims[2]     = {50, 20};  /* Dataset dimension sizes */
    hsize_t     max_dims[2] =            /* Dataset maximum dimension sizes */
        {H5S_UNLIMITED, H5S_UNLIMITED};
    hsize_t                    chunk_dims[2] = {2, 5}; /* Dataset chunked dimension sizes */
    unsigned                   num_entries   = 0;      /* Number of entries in the index */
    H5FD_vfd_swmr_idx_entry_t *index         = NULL;   /* Pointer to the index entries */

    hid_t                  fcpl          = H5I_INVALID_HID; /* File creation property list */
    hid_t                  fid_writer    = H5I_INVALID_HID; /* File ID for writer */
    hid_t                  fapl_writer   = H5I_INVALID_HID; /* File access property list for writer */
    H5F_vfd_swmr_config_t *config_writer = NULL;            /* VFD SWMR Configuration for writer */
    pid_t                  tmppid;                          /* Child process ID returned by waitpid */
    pid_t                  childpid = 0;                    /* Child process ID */
    int                    child_status;                    /* Status passed to waitpid */
    int                    child_wait_option = 0;           /* Options passed to waitpid */
    int                    child_exit_val;                  /* Exit status of the child */

    int    parent_pfd[2]; /* Pipe for parent process as writer */
    int    child_pfd[2];  /* Pipe for child process as reader */
    int    notify = 0;    /* Notification between parent and child */
    H5F_t *file_writer;   /* File pointer for writer */

    TESTING("Verify the metadata file for VFD SWMR reader");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Allocate memory for the configuration structure */
    if ((config_writer = malloc(sizeof(*config_writer))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Set up VFD SWMR configuration as writer in fapl_writer
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config_writer, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME, NULL);

    if ((fapl_writer = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl_writer, false, false, FS_PAGE_SIZE, config_writer) < 0)
        TEST_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, FS_PAGE_SIZE)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create an HDF5 file with VFD SWMR configured */
    if ((fid_writer = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl_writer)) < 0)
        FAIL_STACK_ERROR;

    /* Close the file */
    if (H5Fclose(fid_writer) < 0)
        FAIL_STACK_ERROR;

    /* Create 2 pipes */
    if (pipe(parent_pfd) < 0)
        FAIL_STACK_ERROR;

    if (pipe(child_pfd) < 0)
        FAIL_STACK_ERROR;

    /* Fork child process */
    if ((childpid = fork()) < 0)
        FAIL_STACK_ERROR;

    /*
     * Child process as reader
     */
    if (childpid == 0) {
        int                        child_notify = 0;               /* Notification between child and parent */
        hid_t                      fid_reader   = H5I_INVALID_HID; /* File ID for reader */
        hid_t                      fapl_reader  = H5I_INVALID_HID; /* File access property list for reader */
        H5F_t                     *file_reader;                    /* File pointer for reader */
        H5F_vfd_swmr_config_t     *config_reader     = NULL;       /* VFD SWMR configuration for reader */
        unsigned                   child_num_entries = 0;          /* Number of entries passed to reader */
        H5FD_vfd_swmr_idx_entry_t *child_index       = NULL;       /* Index passed to reader */

        /* Close unused write end for writer pipe */
        if (HDclose(parent_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        /* Close unused read end for reader pipe */
        if (HDclose(child_pfd[0]) < 0)
            exit(EXIT_FAILURE);

        /* Free unused configuration */
        if (config_writer)
            free(config_writer);

        /*
         * Case A: reader
         *  --verify an empty index
         */

        /* Wait for notification 1 from parent to start verification */
        while (child_notify != 1) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Allocate memory for the configuration structure */
        if ((config_reader = malloc(sizeof(*config_reader))) == NULL)
            exit(EXIT_FAILURE);

        /*
         * Set up VFD SWMR configuration as reader in fapl_reader
         */

        /* config, tick_len, max_lag, presume_posix_semantics, writer,
         * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
         * md_file_path, md_file_name, updater_file_path */
        init_vfd_swmr_config(config_reader, 1, 3, false, false, true, false, true, 256, NULL, MD_FILENAME,
                             NULL);

        if ((fapl_reader = H5Pcopy(orig_fapl)) < 0)
            exit(EXIT_FAILURE);

        /* fapl, use_latest_format,  only_meta_page, page_buf_size, config */
        if (vfd_swmr_fapl_augment(fapl_reader, false, false, FS_PAGE_SIZE, config_reader) < 0)
            exit(EXIT_FAILURE);

        /* Open the test file as reader */
        if ((fid_reader = H5Fopen(filename, H5F_ACC_RDONLY, fapl_reader)) < 0)
            exit(EXIT_FAILURE);

        /* Get file pointer */
        file_reader = H5VL_object(fid_reader);

        /* Read and verify header and an empty index in the metadata file */
        if (H5FD__vfd_swmr_reader_md_test(file_reader->shared->lf, 0, NULL) < 0)
            exit(EXIT_FAILURE);

        /* Send notification 2 to parent that the verification is complete */
        child_notify = 2;
        if (HDwrite(child_pfd[1], &child_notify, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /*
         * Case B: reader
         * --verify index as sent from writer
         */

        /* Wait for notification 3 from parent to start verification */
        while (child_notify != 3) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Read num_entries from writer pipe */
        if (HDread(parent_pfd[0], &child_num_entries, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /* Free previous index */
        if (child_index)
            free(child_index);

        if (child_num_entries) {

            /* Allocate memory for num_entries index */
            if ((child_index = calloc(child_num_entries, sizeof(*child_index))) == NULL)
                exit(EXIT_FAILURE);

            /* Read index from writer pipe */
            if (HDread(parent_pfd[0], child_index, child_num_entries * sizeof(*child_index)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Read and verify the expected header and index info in the
         * metadata file
         */
        if (H5FD__vfd_swmr_reader_md_test(file_reader->shared->lf, child_num_entries, child_index) < 0)
            exit(EXIT_FAILURE);

        /* Send notification 4 to parent that the verification is complete */
        child_notify = 4;
        if (HDwrite(child_pfd[1], &child_notify, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /*
         * Case C: reader
         * --verify index as sent from writer
         */

        /* Wait for notification 5 from parent to start verification */
        while (child_notify != 5) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Read num_entries from writer pipe */
        if (HDread(parent_pfd[0], &child_num_entries, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /* Free previous index */
        if (child_index)
            free(child_index);

        if (child_num_entries) {
            /* Allocate memory for num_entries index */
            if ((child_index = (H5FD_vfd_swmr_idx_entry_t *)calloc(
                     child_num_entries, sizeof(H5FD_vfd_swmr_idx_entry_t))) == NULL)
                exit(EXIT_FAILURE);

            /* Read index from writer pipe */
            if (HDread(parent_pfd[0], child_index, child_num_entries * sizeof(H5FD_vfd_swmr_idx_entry_t)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Read and verify the expected header and index info in the
         * metadata file
         */
        if (H5FD__vfd_swmr_reader_md_test(file_reader->shared->lf, child_num_entries, child_index) < 0)
            exit(EXIT_FAILURE);

        /* Send notification 6 to parent that the verification is complete */
        child_notify = 6;
        if (HDwrite(child_pfd[1], &child_notify, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /*
         * Case D: reader
         * --verify index as sent from writer
         */

        /* Wait for notification 7 from parent to start verification */
        while (child_notify != 7) {

            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)

                exit(EXIT_FAILURE);
        }

        /* Read num_entries from writer pipe */
        if (HDread(parent_pfd[0], &child_num_entries, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /* Free previous index */
        if (child_index)
            free(child_index);

        if (child_num_entries) {
            /* Allocate memory for num_entries index */
            if ((child_index = (H5FD_vfd_swmr_idx_entry_t *)calloc(
                     child_num_entries, sizeof(H5FD_vfd_swmr_idx_entry_t))) == NULL)
                exit(EXIT_FAILURE);

            /* Read index from writer pipe */
            if (HDread(parent_pfd[0], child_index, child_num_entries * sizeof(H5FD_vfd_swmr_idx_entry_t)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Read and verify the expected header and index info in the
         * metadata file
         */
        if (H5FD__vfd_swmr_reader_md_test(file_reader->shared->lf, child_num_entries, child_index) < 0)
            exit(EXIT_FAILURE);

        /* Send notification 8 to parent that the verification is complete */
        child_notify = 8;
        if (HDwrite(child_pfd[1], &child_notify, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /*
         * Case E: reader
         * --verify an empty index
         */

        /* Wait for notification 9 from parent to start verification */
        while (child_notify != 9) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Read and verify header and an empty index in the metadata file */
        if (H5FD__vfd_swmr_reader_md_test(file_reader->shared->lf, 0, NULL) < 0)
            exit(EXIT_FAILURE);

        /* Free resources */
        free(child_index);
        free(config_reader);

        /* Closing */
        if (H5Fclose(fid_reader) < 0)
            exit(EXIT_FAILURE);
        if (H5Pclose(fapl_reader) < 0)
            exit(EXIT_FAILURE);

        /* Close the pipes */
        if (HDclose(parent_pfd[0]) < 0)
            exit(EXIT_FAILURE);
        if (HDclose(child_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        exit(EXIT_SUCCESS);
    } /* end child process */

    /*
     * Parent process as writer
     */

    /* Close unused read end for writer pipe */
    if (HDclose(parent_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Close unused write end for reader pipe */
    if (HDclose(child_pfd[1]) < 0)
        FAIL_STACK_ERROR;

    /*
     * Case A: writer
     * --open the file as VFD SWMR writer
     */

    /* Open as VFD SWMR writer */
    if ((fid_writer = H5Fopen(filename, H5F_ACC_RDWR, fapl_writer)) < 0)
        FAIL_STACK_ERROR;

    /* Get the file pointer */
    file_writer = H5VL_object(fid_writer);

    /* Send notification 1 to reader to start verification */
    notify = 1;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Case B: writer
     *  --create datasets to ensure ticks elapse
     *  --construct 12 entries in the index
     *  --update the metadata file with the index
     */

    /* Wait for notification 2 from reader that the verification is complete */
    while (notify != 2) {
        if (HDread(child_pfd[0], &notify, sizeof(int)) < 0)
            FAIL_STACK_ERROR;
    }

    /* Create dataset creation property list */
    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        FAIL_STACK_ERROR;

    /* Set to use chunked dataset */
    if (H5Pset_chunk(dcpl, 2, chunk_dims) < 0)
        FAIL_STACK_ERROR;

    /* Create dataspace */
    if ((sid = H5Screate_simple(2, dims, max_dims)) < 0)
        FAIL_STACK_ERROR;

    /* Perform activities to ensure that ticks elapse */
    for (i = 0; i < config_writer->max_lag + 1; i++) {
        decisleep(config_writer->tick_len);

        /* Create a chunked dataset */
        sprintf(dname, "dset %d", i);
        if ((did = H5Dcreate2(fid_writer, dname, H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            FAIL_STACK_ERROR;

        /* Get dataset object header address */
        if (H5Oget_info3(did, &oinfo, H5O_INFO_BASIC) < 0)
            FAIL_STACK_ERROR;

        /* Close the dataset */
        if (H5Dclose(did) < 0)
            FAIL_STACK_ERROR;
    }

    num_entries = 12;

    /* Allocate num_entries for the data buffer */
    if ((buf = calloc(num_entries, FS_PAGE_SIZE)) == NULL)
        FAIL_STACK_ERROR;

    /* Allocate memory for num_entries index */
    index = calloc(num_entries, sizeof(H5FD_vfd_swmr_idx_entry_t));
    if (NULL == index)
        FAIL_STACK_ERROR;

    /* Construct index for updating the metadata file */
    for (i = 0; i < num_entries; i++) {
        index[i].hdf5_page_offset    = 3 + 7 * i;
        index[i].md_file_page_offset = 1 + (num_entries - i) * 5;
        index[i].length              = (uint32_t)FS_PAGE_SIZE;
        index[i].entry_ptr           = &buf[i * FS_PAGE_SIZE];
        index[i].tick_of_last_change = file_writer->shared->tick_num;
    }

    /* Update the metadata file with the index */
    if (H5F_update_vfd_swmr_metadata_file(file_writer, num_entries, index) < 0)
        TEST_ERROR;

    /* Send notification 3 to child to start verification */
    notify = 3;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Send num_entries to the reader */
    if (HDwrite(parent_pfd[1], &num_entries, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Send index to the reader */
    if (HDwrite(parent_pfd[1], index, num_entries * sizeof(H5FD_vfd_swmr_idx_entry_t)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Case C: writer
     *  --write to the datasets to ensure ticks elapse
     *  --update 3 entries in the index
     *  --update the metadata file with the index
     */

    /* Wait for notification 4 from reader that the verification is complete */
    while (notify != 4) {
        if (HDread(child_pfd[0], &notify, sizeof(int)) < 0)
            FAIL_STACK_ERROR;
    }

    /* Allocate memory for the read/write buffer */
    if ((rwbuf = malloc(sizeof(*rwbuf) * (50 * 20))) == NULL)
        FAIL_STACK_ERROR;
    for (i = 0; i < (50 * 20); i++)
        rwbuf[i] = (int)i;

    /* Perform activities to ensure that max_lag ticks elapse */
    for (i = 0; i < config_writer->max_lag + 1; i++) {
        decisleep(config_writer->tick_len);

        /* Open the dataset */
        sprintf(dname, "dset %d", i);
        if ((did = H5Dopen2(fid_writer, dname, H5P_DEFAULT)) < 0)
            FAIL_STACK_ERROR;

        /* Write to the dataset */
        if (H5Dwrite(did, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, rwbuf) < 0)
            FAIL_STACK_ERROR;

        /* Close the dataset */
        if (H5Dclose(did) < 0)
            FAIL_STACK_ERROR;
    }

    /* Update 3 entries in the index */
    num_entries = 3;
    for (i = 0; i < num_entries; i++) {
        index[i].entry_ptr           = &buf[i * FS_PAGE_SIZE];
        index[i].tick_of_last_change = file_writer->shared->tick_num;
    }

    /* Update the metadata file with the index */
    if (H5F_update_vfd_swmr_metadata_file(file_writer, num_entries, index) < 0)
        TEST_ERROR;

    /* Send notification 5 to reader to start verification */
    notify = 5;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Send num_entries to the reader */
    if (HDwrite(parent_pfd[1], &num_entries, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Send index to the reader */
    if (HDwrite(parent_pfd[1], index, num_entries * sizeof(H5FD_vfd_swmr_idx_entry_t)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Case D: writer
     *  --read from the datasets to ensure ticks elapse
     *  --update 5 entries in the index
     *  --update the metadata file with the index
     */

    /* Wait for notification 6 from reader that the verification is complete */
    while (notify != 6) {
        if (HDread(child_pfd[0], &notify, sizeof(int)) < 0)
            FAIL_STACK_ERROR;
    }

    /* Perform activities to ensure that max_lag ticks elapse */
    for (i = 0; i < config_writer->max_lag + 1; i++) {
        decisleep(config_writer->tick_len);

        /* Open the dataset */
        sprintf(dname, "dset %d", i);
        if ((did = H5Dopen2(fid_writer, dname, H5P_DEFAULT)) < 0)
            FAIL_STACK_ERROR;

        /* Read from the dataset */
        if (H5Dread(did, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, rwbuf) < 0)
            FAIL_STACK_ERROR;

        /* Close the dataset */
        if (H5Dclose(did) < 0)
            FAIL_STACK_ERROR;
    }

    /* Update 5 entries in the index */
    num_entries = 5;
    for (i = 0; i < num_entries; i++) {
        index[i].entry_ptr           = &buf[i * FS_PAGE_SIZE];
        index[i].tick_of_last_change = file_writer->shared->tick_num;
    }

    /* Update the metadata file with the index */
    if (H5F_update_vfd_swmr_metadata_file(file_writer, num_entries, index) < 0)
        TEST_ERROR;

    /* Send notification 7 to reader to start verification */
    notify = 7;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Send num_entries to the reader */
    if (HDwrite(parent_pfd[1], &num_entries, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Send index to the reader */
    if (HDwrite(parent_pfd[1], index, num_entries * sizeof(H5FD_vfd_swmr_idx_entry_t)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Case E: writer
     * --write to the datasets again to ensure ticks elapse
     * --update the metadata file with an empty index
     */

    /* Wait for notification 8 from reader that the verification is complete */
    while (notify != 8) {
        if (HDread(child_pfd[0], &notify, sizeof(int)) < 0)
            FAIL_STACK_ERROR;
    }

    /* Perform activities to ensure that ticks elapse */
    for (i = 0; i < config_writer->max_lag + 1; i++) {
        decisleep(config_writer->tick_len);

        /* Open the dataset */
        sprintf(dname, "dset %d", i);
        if ((did = H5Dopen2(fid_writer, dname, H5P_DEFAULT)) < 0)
            FAIL_STACK_ERROR;

        /* Write to the dataset */
        if (H5Dwrite(did, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, rwbuf) < 0)
            FAIL_STACK_ERROR;

        /* Close the dataset */
        if (H5Dclose(did) < 0)
            FAIL_STACK_ERROR;
    }

    /* Update the metadata file with 0 entries and NULL index */
    if (H5F_update_vfd_swmr_metadata_file(file_writer, 0, NULL) < 0)
        TEST_ERROR;

    /* Send notification 8 to reader to start verification */
    notify = 9;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Done
     */

    /* Close the pipes */
    if (HDclose(parent_pfd[1]) < 0)
        FAIL_STACK_ERROR;
    if (HDclose(child_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Wait for child process to complete */
    if ((tmppid = waitpid(childpid, &child_status, child_wait_option)) < 0)
        FAIL_STACK_ERROR;

    /* Check exit status of child process */
    if (WIFEXITED(child_status)) {
        if ((child_exit_val = WEXITSTATUS(child_status)) != 0)
            TEST_ERROR;
    }
    else { /* child process terminated abnormally */
        TEST_ERROR;
    }

    /* Closing */
    if (H5Fclose(fid_writer) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl_writer) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free resources */
    free(config_writer);
    free(buf);
    free(rwbuf);
    free(index);

    PASSED();
    return 0;

error:
    free(config_writer);
    free(buf);
    free(rwbuf);
    free(index);

    H5E_BEGIN_TRY
    {
        H5Pclose(fapl_writer);
        H5Fclose(fid_writer);
        H5Pclose(fcpl);
    }
    H5E_END_TRY;

    return 1;
} /* test_reader_md_concur() */

/*-------------------------------------------------------------------------
 * Function:    test_multiple_file_opens_concur()
 *
 * Purpose:     Verify the entries on the EOT queue when opening files
 *              with and without VFD SWMR configured.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; 11/18/2019
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_multiple_file_opens_concur(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN];  /* Filename to use */
    char                   filename2[FILE_NAME_LEN]; /* Filename to use */
    hid_t                  fcpl = H5I_INVALID_HID;
    pid_t                  tmppid;                /* Child process ID returned by waitpid */
    pid_t                  childpid = 0;          /* Child process ID */
    int                    child_status;          /* Status passed to waitpid */
    int                    child_wait_option = 0; /* Options passed to waitpid */
    int                    child_exit_val;        /* Exit status of the child */
    int                    parent_pfd[2];         /* Pipe for parent process as writer */
    int                    child_pfd[2];          /* Pipe for child process as reader */
    int                    notify  = 0;           /* Notification between parent and child */
    hid_t                  fid1    = H5I_INVALID_HID;
    hid_t                  fid2    = H5I_INVALID_HID;
    hid_t                  fapl1   = H5I_INVALID_HID;
    hid_t                  fapl2   = H5I_INVALID_HID;
    H5F_vfd_swmr_config_t *config1 = NULL; /* VFD SWMR configuration */
    H5F_vfd_swmr_config_t *config2 = NULL; /* VFD SWMR configuration */
    H5F_t                 *f1, *f2;        /* File pointer */
    eot_queue_entry_t     *curr;

    TESTING("EOT queue entries when opening files concurrently with VFD SWMR");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));
    h5_fixname(namebase2, orig_fapl, filename2, sizeof(filename2));

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, FS_PAGE_SIZE)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create file A */
    if ((fid1 = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, H5P_DEFAULT)) < 0)
        FAIL_STACK_ERROR;

    /* Close the file */
    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;

    /* Create file B */
    if ((fid2 = H5Fcreate(filename2, H5F_ACC_TRUNC, fcpl, H5P_DEFAULT)) < 0)
        FAIL_STACK_ERROR;

    /* Close the file */
    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /* Create 2 pipes */
    if (pipe(parent_pfd) < 0)
        FAIL_STACK_ERROR;

    if (pipe(child_pfd) < 0)
        FAIL_STACK_ERROR;

    /* Fork child process */
    if ((childpid = fork()) < 0)
        FAIL_STACK_ERROR;

    /*
     * Child process
     */
    if (childpid == 0) {
        int                    child_notify  = 0;               /* Notification between child and parent */
        hid_t                  fid_writer    = H5I_INVALID_HID; /* File ID for writer */
        hid_t                  fapl_writer   = H5I_INVALID_HID; /* File access property list for writer */
        H5F_vfd_swmr_config_t *config_writer = NULL;            /* VFD SWMR configuration for reader */

        /* Close unused write end for writer pipe */
        if (HDclose(parent_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        /* Close unused read end for reader pipe */
        if (HDclose(child_pfd[0]) < 0)
            exit(EXIT_FAILURE);

        /*
         * Set up and open file B as VFD SWMR writer
         */

        /* Wait for notification 1 from parent before opening file B */
        while (child_notify != 1) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Allocate memory for VFD SMWR configuration */
        if ((config_writer = malloc(sizeof(*config_writer))) == NULL)
            exit(EXIT_FAILURE);

        /* Set up VFD SWMR configuration in fapl_writer */

        /* config, tick_len, max_lag, presume_posix_semantics, writer,
         * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
         * md_file_path, md_file_name, updater_file_path */
        init_vfd_swmr_config(config_writer, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME2,
                             NULL);

        if ((fapl_writer = H5Pcopy(orig_fapl)) < 0)
            exit(EXIT_FAILURE);

        /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
        if (vfd_swmr_fapl_augment(fapl_writer, false, false, FS_PAGE_SIZE, config_writer) < 0)
            exit(EXIT_FAILURE);

        /* Open file B as VFD SWMR writer */
        if ((fid_writer = H5Fopen(filename2, H5F_ACC_RDWR, fapl_writer)) < 0)
            exit(EXIT_FAILURE);

        /* Send notification 2 to parent that file B is open */
        child_notify = 2;
        if (HDwrite(child_pfd[1], &child_notify, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /* Wait for notification 3 from parent before closing file B */
        while (child_notify != 3) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        free(config_writer);

        /* Close the file */
        if (H5Fclose(fid_writer) < 0)
            exit(EXIT_FAILURE);
        if (H5Pclose(fapl_writer) < 0)
            exit(EXIT_FAILURE);

        /* Send notification 4 to parent that file B is closed */
        child_notify = 4;
        if (HDwrite(child_pfd[1], &child_notify, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /* Close the pipes */
        if (HDclose(parent_pfd[0]) < 0)
            exit(EXIT_FAILURE);
        if (HDclose(child_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        exit(EXIT_SUCCESS);
    } /* end child process */

    /*
     * Parent process
     */

    /* Close unused read end for writer pipe */
    if (HDclose(parent_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Close unused write end for reader pipe */
    if (HDclose(child_pfd[1]) < 0)
        FAIL_STACK_ERROR;

    /*
     * Set up and open file A as VFD SWMR writer
     */

    /* Allocate memory for VFD SWMR configuration */
    if ((config1 = malloc(sizeof(*config1))) == NULL)
        FAIL_STACK_ERROR;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 7, 10, false, true, true, false, true, 256, NULL, MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, FS_PAGE_SIZE, config1) < 0)
        FAIL_STACK_ERROR;

    /* Open file A as VFD SWMR writer */
    if ((fid1 = H5Fopen(filename, H5F_ACC_RDWR, fapl1)) < 0)
        FAIL_STACK_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f1 = H5VL_object(fid1)))
        FAIL_STACK_ERROR;

    /* Head of EOT queue should be a writer */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || !curr->vfd_swmr_writer)
        TEST_ERROR;

    /* The EOT queue's first entry should be f1 */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || curr->vfd_swmr_shared != f1->shared)
        TEST_ERROR;

    /* Send notification 1 to child to open file B */
    notify = 1;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Wait for notification 2 from child that file B is open */
    while (notify != 2) {
        if (HDread(child_pfd[0], &notify, sizeof(int)) < 0)
            FAIL_STACK_ERROR;
    }

    /* Open file B as VFD SWMR reader */

    /* Allocate memory for VFD SWMR configuration */
    if ((config2 = malloc(sizeof(*config2))) == NULL)
        FAIL_STACK_ERROR;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config2, 1, 3, false, false, true, false, true, 256, NULL, MD_FILENAME2, NULL);

    if ((fapl2 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl2, false, false, FS_PAGE_SIZE, config2) < 0)
        FAIL_STACK_ERROR;

    /* Open file B as VFD SWMR reader */
    if ((fid2 = H5Fopen(filename2, H5F_ACC_RDONLY, fapl2)) < 0)
        FAIL_STACK_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f2 = H5VL_object(fid2)))
        FAIL_STACK_ERROR;

    /* Head of EOT queue should NOT be a writer */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) != NULL && curr->vfd_swmr_writer)
        TEST_ERROR;

    /* The EOT queue's first entry should be f2 */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || curr->vfd_swmr_shared != f2->shared)
        TEST_ERROR;

    /* Send notification 3 to child to close file B */
    notify = 3;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Wait for notification 4 from child that file B is closed */
    while (notify != 4) {
        if (HDread(child_pfd[0], &notify, sizeof(int)) < 0)
            FAIL_STACK_ERROR;
    }

    /*
     * Done
     */

    /* Close the pipes */
    if (HDclose(parent_pfd[1]) < 0)
        FAIL_STACK_ERROR;
    if (HDclose(child_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Wait for child process to complete */
    if ((tmppid = waitpid(childpid, &child_status, child_wait_option)) < 0)
        FAIL_STACK_ERROR;

    /* Check exit status of child process */
    if (WIFEXITED(child_status)) {
        if ((child_exit_val = WEXITSTATUS(child_status)) != 0)
            TEST_ERROR;
    }
    else { /* child process terminated abnormally */
        TEST_ERROR;
    }

    /* Closing */
    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl2) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free resources */
    free(config1);
    free(config2);

    PASSED();
    return 0;

error:
    free(config1);
    free(config2);

    H5E_BEGIN_TRY
    {
        H5Pclose(fapl1);
        H5Pclose(fapl2);
        H5Fclose(fid1);
        H5Fclose(fid2);
        H5Pclose(fcpl);
    }
    H5E_END_TRY;

    return 1;
} /* test_multiple_file_opens_concur() */

/*-------------------------------------------------------------------------
 * Function:    test_enable_disable_eot_concur()
 *
 * Purpose:     Verify the public routines:
 *                  H5Fvfd_swmr_enable_end_of_tick()
 *                  H5Fvfd_swmr_disable_end_of_tick()
 *              enables/disables EOT when the files are opened
 *              concurrently.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; June 2020
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_enable_disable_eot_concur(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN];          /* Filename to use */
    char                   filename2[FILE_NAME_LEN];         /* Filename to use */
    char                   filename3[FILE_NAME_LEN];         /* Filename to use */
    hid_t                  fcpl           = H5I_INVALID_HID; /* File creation property list */
    hid_t                  fid_writer     = H5I_INVALID_HID; /* File ID for writer (filename) */
    hid_t                  fid_writer2    = H5I_INVALID_HID; /* File ID for writer (filename2) */
    hid_t                  fid_writer3    = H5I_INVALID_HID; /* File ID for writer (filename3) */
    hid_t                  fapl_writer    = H5I_INVALID_HID; /* FAPL for writer (filename) */
    hid_t                  fapl_writer2   = H5I_INVALID_HID; /* FAPL for writer (filename2) */
    hid_t                  fapl_writer3   = H5I_INVALID_HID; /* FAPL for writer (filename3) */
    H5F_vfd_swmr_config_t *config_writer  = NULL; /* VFD SWMR Configuration for writer (filename) */
    H5F_vfd_swmr_config_t *config_writer2 = NULL; /* VFD SWMR Configuration for writer (filename2) */
    H5F_vfd_swmr_config_t *config_writer3 = NULL; /* VFD SWMR Configuration for writer (filename3) */
    pid_t                  tmppid;                /* Child process ID returned by waitpid */
    pid_t                  childpid = 0;          /* Child process ID */
    int                    child_status;          /* Status passed to waitpid */
    int                    child_wait_option = 0; /* Options passed to waitpid */
    int                    child_exit_val;        /* Exit status of the child */

    int parent_pfd[2]; /* Pipe for parent process as writer */
    int child_pfd[2];  /* Pipe for child process as reader */
    int notify = 0;    /* Notification between parent and child */

    TESTING("Verify concurrent H5Fvfd_swmr_enable/disable_end_of_tick()");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));
    h5_fixname(namebase2, orig_fapl, filename2, sizeof(filename2));
    h5_fixname(namebase3, orig_fapl, filename3, sizeof(filename3));

    /*
     * Set up 3 distinct VFD SWMR files (filename, filename2, filename3).
     *
     * Per the VFD SWMR RFC (section 3.2.2), there is exactly one EOT queue
     * entry per *underlying shared file* (H5F_file_t), not per H5Fopen()
     * call: reopening the same file from the same process shares one
     * H5F_shared_t and therefore one EOT queue entry (this is also required
     * to avoid leaking an eot_queue_entry_t per redundant reopen -- see the
     * H5F_vfd_swmr_insert_entry_eot() call site in H5Fint.c). To exercise
     * "3 independent EOT queue entries" below, this test needs 3 independent
     * files, not the same file opened 3 times.
     *
     * Each of the 3 files also needs a *live* concurrent writer while the
     * child performs its reader opens below: a VFD SWMR reader open of a
     * file that was merely created-then-closed, with no concurrent writer,
     * hangs rather than completing (the open handshake appears to depend on
     * an actively-ticking writer). filename's writer is reopened by the
     * parent after fork(), matching the original single-file test; filename2
     * and filename3 need the same treatment, so their writer config/fapl are
     * kept alive (not freed here) for that reopen below.
     */

    /* Allocate memory for the configuration structures, one per file */
    if ((config_writer = malloc(sizeof(*config_writer))) == NULL)
        FAIL_STACK_ERROR;
    if ((config_writer2 = malloc(sizeof(*config_writer2))) == NULL)
        FAIL_STACK_ERROR;
    if ((config_writer3 = malloc(sizeof(*config_writer3))) == NULL)
        FAIL_STACK_ERROR;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config_writer, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME, NULL);
    init_vfd_swmr_config(config_writer2, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME2, NULL);
    init_vfd_swmr_config(config_writer3, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME3, NULL);

    if ((fapl_writer = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;
    if ((fapl_writer2 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;
    if ((fapl_writer3 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl_writer, false, false, FS_PAGE_SIZE, config_writer) < 0)
        FAIL_STACK_ERROR;
    if (vfd_swmr_fapl_augment(fapl_writer2, false, false, FS_PAGE_SIZE, config_writer2) < 0)
        FAIL_STACK_ERROR;
    if (vfd_swmr_fapl_augment(fapl_writer3, false, false, FS_PAGE_SIZE, config_writer3) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, FS_PAGE_SIZE)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create the 3 HDF5 files with VFD SWMR configured, then close them */
    if ((fid_writer = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl_writer)) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer) < 0)
        FAIL_STACK_ERROR;

    if ((fid_writer2 = H5Fcreate(filename2, H5F_ACC_TRUNC, fcpl, fapl_writer2)) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer2) < 0)
        FAIL_STACK_ERROR;

    if ((fid_writer3 = H5Fcreate(filename3, H5F_ACC_TRUNC, fcpl, fapl_writer3)) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer3) < 0)
        FAIL_STACK_ERROR;

    /* Create 2 pipes */
    if (pipe(parent_pfd) < 0)
        FAIL_STACK_ERROR;

    if (pipe(child_pfd) < 0)
        FAIL_STACK_ERROR;

    /* Fork child process */
    if ((childpid = fork()) < 0)
        FAIL_STACK_ERROR;

    /*
     * Child process as reader
     */
    if (childpid == 0) {
        int                    child_notify   = 0;               /* Notification between child and parent */
        hid_t                  fid_reader     = H5I_INVALID_HID; /* File ID for reader (filename) */
        hid_t                  fid_reader2    = H5I_INVALID_HID; /* File ID for reader (filename2) */
        hid_t                  fid_reader3    = H5I_INVALID_HID; /* File ID for reader (filename3) */
        hid_t                  fapl_reader    = H5I_INVALID_HID; /* FAPL for reader (filename) */
        hid_t                  fapl_reader2   = H5I_INVALID_HID; /* FAPL for reader (filename2) */
        hid_t                  fapl_reader3   = H5I_INVALID_HID; /* FAPL for reader (filename3) */
        H5F_vfd_swmr_config_t *config_reader  = NULL;            /* VFD SWMR configuration (filename) */
        H5F_vfd_swmr_config_t *config_reader2 = NULL;            /* VFD SWMR configuration (filename2) */
        H5F_vfd_swmr_config_t *config_reader3 = NULL;            /* VFD SWMR configuration (filename3) */
        H5F_t                 *file_reader;                      /* File pointer */
        eot_queue_entry_t     *curr;                             /* Pointer to an entry on the EOT queue */
        unsigned               count = 0;                        /* Counter */

        /* Close unused write end for writer pipe */
        if (HDclose(parent_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        /* Close unused read end for reader pipe */
        if (HDclose(child_pfd[0]) < 0)
            exit(EXIT_FAILURE);

        /* Free unused configuration */
        if (config_writer)
            free(config_writer);
        if (config_writer2)
            free(config_writer2);
        if (config_writer3)
            free(config_writer3);

        /*
         *  Open 3 distinct files as VFD SWMR reader
         *  Enable and disable EOT for a file
         *  Verify the state of the EOT queue
         */

        /* Wait for notification 1 from parent to start verification */
        while (child_notify != 1) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Allocate memory for the configuration structures, one per file */
        if ((config_reader = malloc(sizeof(*config_reader))) == NULL)
            exit(EXIT_FAILURE);
        if ((config_reader2 = malloc(sizeof(*config_reader2))) == NULL)
            exit(EXIT_FAILURE);
        if ((config_reader3 = malloc(sizeof(*config_reader3))) == NULL)
            exit(EXIT_FAILURE);

        /*
         * Set up the VFD SWMR configuration as reader + page buffering,
         * one per file (each config must reference that file's own
         * md_file_name)
         */

        /* config, tick_len, max_lag, presume_posix_semantics, writer,
         * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
         * md_file_path, md_file_name, updater_file_path */
        init_vfd_swmr_config(config_reader, 1, 3, false, false, true, false, true, 256, NULL, MD_FILENAME,
                             NULL);
        init_vfd_swmr_config(config_reader2, 1, 3, false, false, true, false, true, 256, NULL, MD_FILENAME2,
                             NULL);
        init_vfd_swmr_config(config_reader3, 1, 3, false, false, true, false, true, 256, NULL, MD_FILENAME3,
                             NULL);

        if ((fapl_reader = H5Pcopy(orig_fapl)) < 0)
            exit(EXIT_FAILURE);
        if ((fapl_reader2 = H5Pcopy(orig_fapl)) < 0)
            exit(EXIT_FAILURE);
        if ((fapl_reader3 = H5Pcopy(orig_fapl)) < 0)
            exit(EXIT_FAILURE);

        /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
        if (vfd_swmr_fapl_augment(fapl_reader, false, false, FS_PAGE_SIZE, config_reader) < 0)
            exit(EXIT_FAILURE);
        if (vfd_swmr_fapl_augment(fapl_reader2, false, false, FS_PAGE_SIZE, config_reader2) < 0)
            exit(EXIT_FAILURE);
        if (vfd_swmr_fapl_augment(fapl_reader3, false, false, FS_PAGE_SIZE, config_reader3) < 0)
            exit(EXIT_FAILURE);

        /* Open the 3 distinct files as reader */
        if ((fid_reader = H5Fopen(filename, H5F_ACC_RDONLY, fapl_reader)) < 0)
            exit(EXIT_FAILURE);

        if ((fid_reader2 = H5Fopen(filename2, H5F_ACC_RDONLY, fapl_reader2)) < 0)
            exit(EXIT_FAILURE);

        if ((fid_reader3 = H5Fopen(filename3, H5F_ACC_RDONLY, fapl_reader3)) < 0)
            exit(EXIT_FAILURE);

        /* Verify the # of files on the EOT queue is 3 */
        count = 0;
        TAILQ_FOREACH(curr, &eot_queue_g, link)
        count++;
        if (count != 3)
            exit(EXIT_FAILURE);

        /* Disable EOT for the second opened file */
        if (H5Fvfd_swmr_disable_end_of_tick(fid_reader2) < 0)
            exit(EXIT_FAILURE);

        /* Verify the # of files on the EOT queue is 2 */
        count = 0;
        TAILQ_FOREACH(curr, &eot_queue_g, link)
        count++;
        if (count != 2)
            exit(EXIT_FAILURE);

        /* Get file pointer */
        file_reader = H5VL_object(fid_reader2);

        /* Should not find the second opened file on the EOT queue */
        TAILQ_FOREACH(curr, &eot_queue_g, link)
        {
            if (curr->vfd_swmr_shared == file_reader->shared)
                break;
        }
        if (curr != NULL && curr->vfd_swmr_shared == file_reader->shared)
            exit(EXIT_FAILURE);

        /* Enable EOT for the second opened file again */
        if (H5Fvfd_swmr_enable_end_of_tick(fid_reader2) < 0)
            exit(EXIT_FAILURE);

        /* Verify the # of files on the EOT queue is 3 */
        count = 0;
        TAILQ_FOREACH(curr, &eot_queue_g, link)
        count++;
        if (count != 3)
            exit(EXIT_FAILURE);

        /* Should find the second opened file on the EOT queue */
        TAILQ_FOREACH(curr, &eot_queue_g, link)
        {
            if (curr->vfd_swmr_shared == file_reader->shared)
                break;
        }
        if (curr == NULL || curr->vfd_swmr_shared != file_reader->shared)
            exit(EXIT_FAILURE);

        /* Closing */
        if (H5Fclose(fid_reader) < 0)
            exit(EXIT_FAILURE);
        if (H5Fclose(fid_reader2) < 0)
            exit(EXIT_FAILURE);
        if (H5Fclose(fid_reader3) < 0)
            exit(EXIT_FAILURE);
        if (H5Pclose(fapl_reader) < 0)
            exit(EXIT_FAILURE);
        if (H5Pclose(fapl_reader2) < 0)
            exit(EXIT_FAILURE);
        if (H5Pclose(fapl_reader3) < 0)
            exit(EXIT_FAILURE);

        free(config_reader);
        free(config_reader2);
        free(config_reader3);

        /* Close the pipes */
        if (HDclose(parent_pfd[0]) < 0)
            exit(EXIT_FAILURE);
        if (HDclose(child_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        exit(EXIT_SUCCESS);
    } /* end child process */

    /*
     * Parent process as writer
     */

    /* Close unused read end for writer pipe */
    if (HDclose(parent_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Close unused write end for reader pipe */
    if (HDclose(child_pfd[1]) < 0)
        FAIL_STACK_ERROR;

    /*
     * Open all 3 files as VFD SWMR writer, so each has a live writer while
     * the child's reader opens run (see the comment above the fork() call).
     */
    if ((fid_writer = H5Fopen(filename, H5F_ACC_RDWR, fapl_writer)) < 0)
        FAIL_STACK_ERROR;
    if ((fid_writer2 = H5Fopen(filename2, H5F_ACC_RDWR, fapl_writer2)) < 0)
        FAIL_STACK_ERROR;
    if ((fid_writer3 = H5Fopen(filename3, H5F_ACC_RDWR, fapl_writer3)) < 0)
        FAIL_STACK_ERROR;

    /* Send notification 1 to reader to start verification */
    notify = 1;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Done
     */

    /* Close the pipes */
    if (HDclose(parent_pfd[1]) < 0)
        FAIL_STACK_ERROR;
    if (HDclose(child_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Wait for child process to complete */
    if ((tmppid = waitpid(childpid, &child_status, child_wait_option)) < 0)
        FAIL_STACK_ERROR;

    /* Check exit status of child process */
    if (WIFEXITED(child_status)) {
        if ((child_exit_val = WEXITSTATUS(child_status)) != 0)
            TEST_ERROR;
    }
    else { /* child process terminated abnormally */
        TEST_ERROR;
    }

    /* Closing */
    if (H5Fclose(fid_writer) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer2) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer3) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl_writer) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl_writer2) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl_writer3) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free resources */
    free(config_writer);
    free(config_writer2);
    free(config_writer3);

    PASSED();
    return 0;

error:
    free(config_writer);
    free(config_writer2);
    free(config_writer3);

    H5E_BEGIN_TRY
    {
        H5Pclose(fapl_writer);
        H5Pclose(fapl_writer2);
        H5Pclose(fapl_writer3);
        H5Fclose(fid_writer);
        H5Fclose(fid_writer2);
        H5Fclose(fid_writer3);
        H5Pclose(fcpl);
    }
    H5E_END_TRY;

    return 1;
} /* test_enable_disable_eot_concur() */

/*-------------------------------------------------------------------------
 * Function:    test_file_end_tick_concur()
 *
 * Purpose:     Verify the public routine H5Fvfd_swmr_end_tick()
 *              triggers end of tick processing when the files
 *              are opened concurrently.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; June 2020
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_file_end_tick_concur(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN];          /* Filename to use */
    char                   filename2[FILE_NAME_LEN];         /* Filename to use */
    char                   filename3[FILE_NAME_LEN];         /* Filename to use */
    hid_t                  fcpl           = H5I_INVALID_HID; /* File creation property list */
    hid_t                  fid_writer     = H5I_INVALID_HID; /* File ID for writer (filename) */
    hid_t                  fid_writer2    = H5I_INVALID_HID; /* File ID for writer (filename2) */
    hid_t                  fid_writer3    = H5I_INVALID_HID; /* File ID for writer (filename3) */
    hid_t                  fapl_writer    = H5I_INVALID_HID; /* FAPL for writer (filename) */
    hid_t                  fapl_writer2   = H5I_INVALID_HID; /* FAPL for writer (filename2) */
    hid_t                  fapl_writer3   = H5I_INVALID_HID; /* FAPL for writer (filename3) */
    H5F_vfd_swmr_config_t *config_writer  = NULL; /* VFD SWMR Configuration for writer (filename) */
    H5F_vfd_swmr_config_t *config_writer2 = NULL; /* VFD SWMR Configuration for writer (filename2) */
    H5F_vfd_swmr_config_t *config_writer3 = NULL; /* VFD SWMR Configuration for writer (filename3) */
    pid_t                  tmppid;                /* Child process ID returned by waitpid */
    pid_t                  childpid = 0;          /* Child process ID */
    int                    child_status;          /* Status passed to waitpid */
    int                    child_wait_option = 0; /* Options passed to waitpid */
    int                    child_exit_val;        /* Exit status of the child */

    int parent_pfd[2]; /* Pipe for parent process as writer */
    int child_pfd[2];  /* Pipe for child process as reader */
    int notify = 0;    /* Notification between parent and child */

    TESTING("Verify concurrent H5Fvfd_swmr_end_tick()");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));
    h5_fixname(namebase2, orig_fapl, filename2, sizeof(filename2));
    h5_fixname(namebase3, orig_fapl, filename3, sizeof(filename3));

    /*
     * Set up 3 distinct VFD SWMR files (filename, filename2, filename3).
     *
     * This test originally opened the *same* file 3 times and called
     * H5Fvfd_swmr_end_tick() on each handle in turn. That does not work: the
     * EOT queue entry inserted at open time (H5F_vfd_swmr_insert_entry_eot(),
     * gated on nrefs==1 -- see H5Fint.c) stores the *first* open's H5F_t
     * pointer, not the shared H5F_shared_t. H5F__vfd_swmr_end_tick() searches
     * the queue by exact H5F_t pointer identity, so calling it on any handle
     * other than the one that happened to trigger the original insert fails
     * with "EOT for the file has been disabled" even though the file's EOT
     * is not actually disabled -- the search just never matches a *different*
     * per-open H5F_t sharing the same underlying file. 3 independent files
     * (as used here) sidesteps this identity ambiguity entirely: each has
     * its own H5F_t and its own EOT queue entry. As in
     * test_enable_disable_eot_concur(), each file also needs a live
     * concurrent writer for its reader open to complete (see that function's
     * comment for why).
     */

    /* Allocate memory for the configuration structures, one per file */
    if ((config_writer = malloc(sizeof(*config_writer))) == NULL)
        FAIL_STACK_ERROR;
    if ((config_writer2 = malloc(sizeof(*config_writer2))) == NULL)
        FAIL_STACK_ERROR;
    if ((config_writer3 = malloc(sizeof(*config_writer3))) == NULL)
        FAIL_STACK_ERROR;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config_writer, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME, NULL);
    init_vfd_swmr_config(config_writer2, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME2, NULL);
    init_vfd_swmr_config(config_writer3, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME3, NULL);

    if ((fapl_writer = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;
    if ((fapl_writer2 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;
    if ((fapl_writer3 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl_writer, false, false, FS_PAGE_SIZE, config_writer) < 0)
        FAIL_STACK_ERROR;
    if (vfd_swmr_fapl_augment(fapl_writer2, false, false, FS_PAGE_SIZE, config_writer2) < 0)
        FAIL_STACK_ERROR;
    if (vfd_swmr_fapl_augment(fapl_writer3, false, false, FS_PAGE_SIZE, config_writer3) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, FS_PAGE_SIZE)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create the 3 HDF5 files with VFD SWMR configured, then close them */
    if ((fid_writer = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl_writer)) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer) < 0)
        FAIL_STACK_ERROR;

    if ((fid_writer2 = H5Fcreate(filename2, H5F_ACC_TRUNC, fcpl, fapl_writer2)) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer2) < 0)
        FAIL_STACK_ERROR;

    if ((fid_writer3 = H5Fcreate(filename3, H5F_ACC_TRUNC, fcpl, fapl_writer3)) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer3) < 0)
        FAIL_STACK_ERROR;

    /* Create 2 pipes */
    if (pipe(parent_pfd) < 0)
        FAIL_STACK_ERROR;

    if (pipe(child_pfd) < 0)
        FAIL_STACK_ERROR;

    /* Fork child process */
    if ((childpid = fork()) < 0)
        FAIL_STACK_ERROR;

    /*
     * Child process as reader
     */
    if (childpid == 0) {
        int                    child_notify   = 0;               /* Notification between child and parent */
        hid_t                  fid_reader1    = H5I_INVALID_HID; /* File ID for reader (filename) */
        hid_t                  fid_reader2    = H5I_INVALID_HID; /* File ID for reader (filename2) */
        hid_t                  fid_reader3    = H5I_INVALID_HID; /* File ID for reader (filename3) */
        hid_t                  fapl_reader    = H5I_INVALID_HID; /* FAPL for reader (filename) */
        hid_t                  fapl_reader2   = H5I_INVALID_HID; /* FAPL for reader (filename2) */
        hid_t                  fapl_reader3   = H5I_INVALID_HID; /* FAPL for reader (filename3) */
        H5F_vfd_swmr_config_t *config_reader  = NULL;            /* VFD SWMR configuration (filename) */
        H5F_vfd_swmr_config_t *config_reader2 = NULL;            /* VFD SWMR configuration (filename2) */
        H5F_vfd_swmr_config_t *config_reader3 = NULL;            /* VFD SWMR configuration (filename3) */
        H5F_t                 *f1, *f2, *f3;                     /* File pointer */
        uint64_t               s1 = 0;                           /* Saved tick_num */
        uint64_t               s2 = 0;                           /* Saved tick_num */
        uint64_t               s3 = 0;                           /* Saved tick_num */

        /* Close unused write end for writer pipe */
        if (HDclose(parent_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        /* Close unused read end for reader pipe */
        if (HDclose(child_pfd[0]) < 0)
            exit(EXIT_FAILURE);

        /* Free unused configuration */
        if (config_writer)
            free(config_writer);
        if (config_writer2)
            free(config_writer2);
        if (config_writer3)
            free(config_writer3);

        /*
         *  Open 3 distinct files as VFD SWMR reader
         *  Trigger EOT for the files
         */

        /* Wait for notification 1 from parent to start verification */
        while (child_notify != 1) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Allocate memory for the configuration structures, one per file */
        if ((config_reader = malloc(sizeof(*config_reader))) == NULL)
            exit(EXIT_FAILURE);
        if ((config_reader2 = malloc(sizeof(*config_reader2))) == NULL)
            exit(EXIT_FAILURE);
        if ((config_reader3 = malloc(sizeof(*config_reader3))) == NULL)
            exit(EXIT_FAILURE);

        /* config, tick_len, max_lag, presume_posix_semantics, writer,
         * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
         * md_file_path, md_file_name, updater_file_path */
        init_vfd_swmr_config(config_reader, 1, 3, false, false, true, false, true, 256, NULL, MD_FILENAME,
                             NULL);
        init_vfd_swmr_config(config_reader2, 1, 3, false, false, true, false, true, 256, NULL, MD_FILENAME2,
                             NULL);
        init_vfd_swmr_config(config_reader3, 1, 3, false, false, true, false, true, 256, NULL, MD_FILENAME3,
                             NULL);

        if ((fapl_reader = H5Pcopy(orig_fapl)) < 0)
            FAIL_STACK_ERROR;
        if ((fapl_reader2 = H5Pcopy(orig_fapl)) < 0)
            FAIL_STACK_ERROR;
        if ((fapl_reader3 = H5Pcopy(orig_fapl)) < 0)
            FAIL_STACK_ERROR;

        /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
        if (vfd_swmr_fapl_augment(fapl_reader, false, false, FS_PAGE_SIZE, config_reader) < 0)
            FAIL_STACK_ERROR;
        if (vfd_swmr_fapl_augment(fapl_reader2, false, false, FS_PAGE_SIZE, config_reader2) < 0)
            FAIL_STACK_ERROR;
        if (vfd_swmr_fapl_augment(fapl_reader3, false, false, FS_PAGE_SIZE, config_reader3) < 0)
            FAIL_STACK_ERROR;

        /* Open the 3 distinct files as reader */
        if ((fid_reader1 = H5Fopen(filename, H5F_ACC_RDONLY, fapl_reader)) < 0)
            exit(EXIT_FAILURE);

        if ((fid_reader2 = H5Fopen(filename2, H5F_ACC_RDONLY, fapl_reader2)) < 0)
            exit(EXIT_FAILURE);

        if ((fid_reader3 = H5Fopen(filename3, H5F_ACC_RDONLY, fapl_reader3)) < 0)
            exit(EXIT_FAILURE);

        /* Get file pointer */
        f1 = H5VL_object(fid_reader1);
        f2 = H5VL_object(fid_reader2);
        f3 = H5VL_object(fid_reader3);

        /* Saved tick_num for the 3 files */
        s1 = f1->shared->tick_num;
        s2 = f2->shared->tick_num;
        s3 = f3->shared->tick_num;

        /* Trigger EOT for the second opened file */
        if (H5Fvfd_swmr_end_tick(fid_reader2) < 0)
            exit(EXIT_FAILURE);

        /* Verify tick_num should not be less than the previous tick_num */
        if (f2->shared->tick_num < s2)
            exit(EXIT_FAILURE);

        if (H5Fclose(fid_reader2) < 0)
            exit(EXIT_FAILURE);

        /* Trigger EOT for the first opened file */
        if (H5Fvfd_swmr_end_tick(fid_reader1) < 0)
            exit(EXIT_FAILURE);

        /* Verify tick_num should not be less than the previous tick_num */
        if (f1->shared->tick_num < s1)
            exit(EXIT_FAILURE);

        if (H5Fclose(fid_reader1) < 0)
            exit(EXIT_FAILURE);

        /* Trigger end tick processing for the third opened file */
        if (H5Fvfd_swmr_end_tick(fid_reader3) < 0)
            exit(EXIT_FAILURE);

        /* Verify tick_num should not be less than the previous tick_num */
        if (f3->shared->tick_num < s3)
            exit(EXIT_FAILURE);

        if (H5Fclose(fid_reader3) < 0)
            exit(EXIT_FAILURE);

        if (H5Pclose(fapl_reader) < 0)
            exit(EXIT_FAILURE);
        if (H5Pclose(fapl_reader2) < 0)
            exit(EXIT_FAILURE);
        if (H5Pclose(fapl_reader3) < 0)
            exit(EXIT_FAILURE);

        free(config_reader);
        free(config_reader2);
        free(config_reader3);

        /* Close the pipes */
        if (HDclose(parent_pfd[0]) < 0)
            exit(EXIT_FAILURE);
        if (HDclose(child_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        exit(EXIT_SUCCESS);
    } /* end child process */

    /*
     * Parent process as writer
     */

    /* Close unused read end for writer pipe */
    if (HDclose(parent_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Close unused write end for reader pipe */
    if (HDclose(child_pfd[1]) < 0)
        FAIL_STACK_ERROR;

    /*
     * Open all 3 files as VFD SWMR writer, so each has a live writer while
     * the child's reader opens run (see the comment above the fork() call).
     */
    if ((fid_writer = H5Fopen(filename, H5F_ACC_RDWR, fapl_writer)) < 0)
        FAIL_STACK_ERROR;
    if ((fid_writer2 = H5Fopen(filename2, H5F_ACC_RDWR, fapl_writer2)) < 0)
        FAIL_STACK_ERROR;
    if ((fid_writer3 = H5Fopen(filename3, H5F_ACC_RDWR, fapl_writer3)) < 0)
        FAIL_STACK_ERROR;

    /* Send notification 1 to reader to start verification */
    notify = 1;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Done
     */

    /* Close the pipes */
    if (HDclose(parent_pfd[1]) < 0)
        FAIL_STACK_ERROR;
    if (HDclose(child_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Wait for child process to complete */
    if ((tmppid = waitpid(childpid, &child_status, child_wait_option)) < 0)
        FAIL_STACK_ERROR;

    /* Check exit status of child process */
    if (WIFEXITED(child_status)) {
        if ((child_exit_val = WEXITSTATUS(child_status)) != 0)
            TEST_ERROR;
    }
    else { /* child process terminated abnormally */
        TEST_ERROR;
    }

    /* Closing */
    if (H5Fclose(fid_writer) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer2) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid_writer3) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl_writer) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl_writer2) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl_writer3) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free resources */
    free(config_writer);
    free(config_writer2);
    free(config_writer3);

    PASSED();
    return 0;

error:
    free(config_writer);
    free(config_writer2);
    free(config_writer3);

    H5E_BEGIN_TRY
    {
        H5Pclose(fapl_writer);
        H5Pclose(fapl_writer2);
        H5Pclose(fapl_writer3);
        H5Fclose(fid_writer);
        H5Fclose(fid_writer2);
        H5Fclose(fid_writer3);
        H5Pclose(fcpl);
    }
    H5E_END_TRY;

    return 1;
} /* test_file_end_tick_concur() */

/*-------------------------------------------------------------------------
 * Function:    test_make_believe_multiple_file_opens_concur()
 *
 * Purpose:     Verify the following:
 *              A) Open a file as VFD SWMR reader
 *                 --there is no metadata file i.e. make-believe is enabled
 *              B) Open the file as VFD SWMR writer
 *                 --metadata file is created
 *              C) Trigger end of tick processing for the reader in (A)
 *                 --Verify make-believe is disabled
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; 4/25/2022
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_make_believe_multiple_file_opens_concur(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN]; /* Filename to use */
    hid_t                  fcpl = H5I_INVALID_HID;
    pid_t                  tmppid;                /* Child process ID returned by waitpid */
    pid_t                  childpid = 0;          /* Child process ID */
    int                    child_status;          /* Status passed to waitpid */
    int                    child_wait_option = 0; /* Options passed to waitpid */
    int                    child_exit_val;        /* Exit status of the child */
    int                    parent_pfd[2];         /* Pipe for parent process as writer */
    int                    child_pfd[2];          /* Pipe for child process as reader */
    int                    notify = 0;            /* Notification between parent and child */
    hid_t                  fid    = H5I_INVALID_HID;
    hid_t                  fapl   = H5I_INVALID_HID;
    H5F_vfd_swmr_config_t *config = NULL; /* VFD SWMR configuration */
    H5F_t                 *f;             /* File pointer */

    TESTING("Verify make-believe-data when opening files concurrently as VFD SWMR reader and then as VFD "
            "SWMR writer");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, FS_PAGE_SIZE)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create file A */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, H5P_DEFAULT)) < 0)
        FAIL_STACK_ERROR;

    /* Close the file */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Create 2 pipes */
    if (pipe(parent_pfd) < 0)
        FAIL_STACK_ERROR;

    if (pipe(child_pfd) < 0)
        FAIL_STACK_ERROR;

    /* Fork child process */
    if ((childpid = fork()) < 0)
        FAIL_STACK_ERROR;

    /*
     * Child process
     */
    if (childpid == 0) {
        int                    child_notify  = 0;               /* Notification between child and parent */
        hid_t                  fid_writer    = H5I_INVALID_HID; /* File ID for writer */
        hid_t                  fapl_writer   = H5I_INVALID_HID; /* File access property list for writer */
        H5F_vfd_swmr_config_t *config_writer = NULL;            /* VFD SWMR configuration for reader */

        /* Close unused write end for writer pipe */
        if (HDclose(parent_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        /* Close unused read end for reader pipe */
        if (HDclose(child_pfd[0]) < 0)
            exit(EXIT_FAILURE);

        /*
         * Set up and open file A as VFD SWMR writer
         */

        /* Wait for notification 1 from parent before opening file A */
        while (child_notify != 1) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        /* Allocate memory for VFD SMWR configuration */
        if ((config_writer = malloc(sizeof(*config_writer))) == NULL)
            exit(EXIT_FAILURE);

        /* Set up VFD SWMR configuration as writer in fapl_writer */

        /* config, tick_len, max_lag, presume_posix_semantics, writer,
         * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
         * md_file_path, md_file_name, updater_file_path */
        init_vfd_swmr_config(config_writer, 1, 3, true, true, true, false, true, 256, NULL, MD_FILENAME,
                             NULL);

        if ((fapl_writer = H5Pcopy(orig_fapl)) < 0)
            exit(EXIT_FAILURE);

        /* use_latest_format, only_meta_page, page_buf_size, config */
        if (vfd_swmr_fapl_augment(fapl_writer, false, false, FS_PAGE_SIZE, config_writer) < 0)
            exit(EXIT_FAILURE);

        /* Open file A as VFD SWMR writer */
        if ((fid_writer = H5Fopen(filename, H5F_ACC_RDWR, fapl_writer)) < 0)
            exit(EXIT_FAILURE);

        /* Send notification 2 to parent that file A is open */
        child_notify = 2;
        if (HDwrite(child_pfd[1], &child_notify, sizeof(int)) < 0)
            exit(EXIT_FAILURE);

        /* Wait for notification 3 from parent before closing file A */
        while (child_notify != 3) {
            if (HDread(parent_pfd[0], &child_notify, sizeof(int)) < 0)
                exit(EXIT_FAILURE);
        }

        free(config_writer);

        /* Close the file */
        if (H5Fclose(fid_writer) < 0)
            exit(EXIT_FAILURE);
        if (H5Pclose(fapl_writer) < 0)
            exit(EXIT_FAILURE);

        /* Close the pipes */
        if (HDclose(parent_pfd[0]) < 0)
            exit(EXIT_FAILURE);
        if (HDclose(child_pfd[1]) < 0)
            exit(EXIT_FAILURE);

        exit(EXIT_SUCCESS);
    } /* end child process */

    /*
     * Parent process
     */

    /* Close unused read end for writer pipe */
    if (HDclose(parent_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Close unused write end for reader pipe */
    if (HDclose(child_pfd[1]) < 0)
        FAIL_STACK_ERROR;

    /*
     * Set up and open file A as VFD SWMR reader
     */

    /* Allocate memory for VFD SWMR configuration */
    if ((config = malloc(sizeof(*config))) == NULL)
        FAIL_STACK_ERROR;

    /* Set up VFD SWMR configuration as reader in fapl */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 7, 10, true, false, true, false, true, 256, NULL, MD_FILENAME, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, FS_PAGE_SIZE, config) < 0)
        FAIL_STACK_ERROR;

    /* Open file A as VFD SWMR reader */
    if ((fid = H5Fopen(filename, H5F_ACC_RDONLY, fapl)) < 0)
        FAIL_STACK_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f = H5VL_object(fid)))
        FAIL_STACK_ERROR;

    /* Verify make-believe: should be true */
    if (!H5FD_vfd_swmr_get_make_believe(f->shared->lf)) {
        printf("Make believe for initial open should be true\n");
        TEST_ERROR;
    }

    /* Send notification 1 to child to open file A as VFD SWMR writer */
    notify = 1;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /* Wait for notification 2 from child that file A is open */
    while (notify != 2) {
        if (HDread(child_pfd[0], &notify, sizeof(int)) < 0)
            FAIL_STACK_ERROR;
    }

    /* Trigger end of tick processing */
    if (H5Fvfd_swmr_end_tick(fid) < 0)
        FAIL_STACK_ERROR;

    /* Verify make-believe: should be false */
    if (H5FD_vfd_swmr_get_make_believe(f->shared->lf)) {
        printf("Make believe after end of tick processing should be false\n");
        TEST_ERROR;
    }

    /* Send notification 3 to child to close file A */
    notify = 3;
    if (HDwrite(parent_pfd[1], &notify, sizeof(int)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Done
     */

    /* Close the pipes */
    if (HDclose(parent_pfd[1]) < 0)
        FAIL_STACK_ERROR;
    if (HDclose(child_pfd[0]) < 0)
        FAIL_STACK_ERROR;

    /* Wait for child process to complete */
    if ((tmppid = waitpid(childpid, &child_status, child_wait_option)) < 0)
        FAIL_STACK_ERROR;

    /* Check exit status of child process */
    if (WIFEXITED(child_status)) {
        if ((child_exit_val = WEXITSTATUS(child_status)) != 0)
            TEST_ERROR;
    }
    else { /* child process terminated abnormally */
        TEST_ERROR;
    }

    /* Closing */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free resources */
    free(config);

    PASSED();
    return 0;

error:
    free(config);

    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
        H5Fclose(fid);
        H5Pclose(fcpl);
    }
    H5E_END_TRY;

    return 1;
} /* test_make_believe_multiple_file_opens_concur() */

#endif /* H5_HAVE_UNISTD_H */

/*-------------------------------------------------------------------------
 * Function:    test_multiple_file_opens()
 *
 * Purpose:     Verify the entries on the EOT queue when opening files
 *              with and without VFD SWMR configured.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; 11/18/2019
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_multiple_file_opens(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN];     /* Filename to use */
    char                   filename2[FILE_NAME_LEN];    /* Filename to use */
    char                   non_filename[FILE_NAME_LEN]; /* Filename to use */
    hid_t                  fid1  = H5I_INVALID_HID;     /* File ID */
    hid_t                  fid2  = H5I_INVALID_HID;     /* File ID */
    hid_t                  fid   = H5I_INVALID_HID;     /* File ID */
    hid_t                  fcpl  = H5I_INVALID_HID;     /* File creation property list ID */
    hid_t                  fapl1 = H5I_INVALID_HID;     /* File access property list ID */
    hid_t                  fapl2 = H5I_INVALID_HID;     /* File access property list ID */
    H5F_t                 *f1, *f2, *f;                 /* File pointer */
    H5F_vfd_swmr_config_t *config1 = NULL;              /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *config2 = NULL;              /* Configuration for VFD SWMR */
    eot_queue_entry_t     *curr;

    TESTING("EOT queue entries when opening files with/without VFD SWMR");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));
    h5_fixname(namebase2, orig_fapl, filename2, sizeof(filename2));
    h5_fixname(non_namebase, orig_fapl, non_filename, sizeof(non_filename));

    /* Allocate memory for the configuration structure */
    if ((config1 = malloc(sizeof(*config1))) == NULL)
        FAIL_STACK_ERROR;
    if ((config2 = malloc(sizeof(*config2))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Configured as VFD SWMR writer + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 6, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config2, 4, 6, false, true, true, false, true, 2, NULL, MD_FILENAME2, NULL);

    if ((fapl2 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl2, false, false, 4096, config2) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create a file without VFD SWMR */
    if ((fid = H5Fcreate(non_filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f = H5VL_object(fid)))
        FAIL_STACK_ERROR;

    /* Verify the global vfd_swmr_writer_g is not set */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) != NULL && curr->vfd_swmr_writer)
        TEST_ERROR;
    /* The EOT queue should be empty */
    if (!TAILQ_EMPTY(&eot_queue_g))
        TEST_ERROR;

    /* Create a file with VFD SWMR writer */
    if ((fid1 = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl1)) < 0)
        TEST_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f1 = H5VL_object(fid1)))
        FAIL_STACK_ERROR;

    /* Head of EOT queue should be a writer */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || !curr->vfd_swmr_writer)
        TEST_ERROR;
    /* The EOT queue should be initialized with the first entry equals to f1 */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || curr->vfd_swmr_shared != f1->shared)
        TEST_ERROR;

    /* Create another file with VFD SWMR writer */
    if ((fid2 = H5Fcreate(filename2, H5F_ACC_TRUNC, fcpl, fapl2)) < 0)
        TEST_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f2 = H5VL_object(fid2)))
        FAIL_STACK_ERROR;

    /* Head of EOT queue should be a writer */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || !curr->vfd_swmr_writer)
        TEST_ERROR;
    /* The EOT queue's first entry should be f1 */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || curr->vfd_swmr_shared != f1->shared)
        TEST_ERROR;

    /* The file without VFD SWMR should not exist on the EOT queue */
    TAILQ_FOREACH(curr, &eot_queue_g, link)
    {
        if (curr->vfd_swmr_shared == f->shared)
            TEST_ERROR;
    }

    /* Close the first file with VFD SWMR */
    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;

    /* Head of EOT queue should be a writer */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || !curr->vfd_swmr_writer)
        TEST_ERROR;
    /* The EOT queue's first entry should be f2 */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) == NULL || curr->vfd_swmr_shared != f2->shared)
        TEST_ERROR;

    /* Close the second file with VFD SWMR */
    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /* Head of EOT queue should not be a writer */
    if ((curr = TAILQ_FIRST(&eot_queue_g)) != NULL && curr->vfd_swmr_writer)
        TEST_ERROR;
    /* The EOT queue should be empty */
    if (!TAILQ_EMPTY(&eot_queue_g))
        TEST_ERROR;

    /* Closing */
    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl2) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config1);
    free(config2);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl1);
        H5Pclose(fapl2);
        H5Pclose(fcpl);
        H5Fclose(fid);
        H5Fclose(fid1);
        H5Fclose(fid2);
    }
    H5E_END_TRY;

    free(config1);
    free(config2);

    return 1;
} /* test_multiple_file_opens() */

/*-------------------------------------------------------------------------
 *  Function:    test_same_file_opens()
 *
 *  Purpose:     Verify multiple opens of the same file as listed below:
 *
 *  When presume is false:
 *
 *                #1st open#
 *  #2nd open#    VW  VR   W   R
 *              ------------------
 *          VW  |  s   f   f   f |
 *          VR  |  f   f   f   f |
 *           W  |  f   f   s   f |
 *           R  |  f   f   s   s |
 *              ------------------
 *
 *  When presume is true: (the only difference is column 2)
 *
 *                #1st open#
 *  #2nd open#    VW  VR   W   R
 *              ------------------
 *          VW  |  s   f   f   f |
 *          VR  |  f   s   f   f |
 *           W  |  f   f   s   f |
 *           R  |  f   f   s   s |
 *              ------------------
 *
 *  Notations:
 *        W:  H5F_ACC_RDWR
 *        R:  H5F_ACC_RDONLY
 *        VW: VFD SWMR writer
 *        VR: VFD SWMR reader
 *
 *        f: the open fails
 *        s: the open succeeds
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2019
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_same_file_opens(hid_t orig_fapl, hbool_t presume)
{
    char                   filename[FILE_NAME_LEN];   /* Filename to use */
    hid_t                  fid     = H5I_INVALID_HID; /* File ID */
    hid_t                  fid2    = H5I_INVALID_HID; /* File ID */
    hid_t                  fcpl    = H5I_INVALID_HID; /* File creation property list ID */
    hid_t                  fapl1   = H5I_INVALID_HID; /* File access property list ID */
    hid_t                  fapl2   = H5I_INVALID_HID; /* File access property list ID */
    H5F_vfd_swmr_config_t *config1 = NULL;            /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *config2 = NULL;            /* Configuration for VFD SWMR */

    TESTING(
        "Multiple opens of the same file with VFD SWMR configuration with/without presume_posix_semantics");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Should succeed without VFD SWMR configured */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Close the file  */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Allocate memory for the configuration structure */
    if ((config1 = malloc(sizeof(*config1))) == NULL)
        FAIL_STACK_ERROR;
    if ((config2 = malloc(sizeof(*config2))) == NULL)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /*
     * Tests for first column
     */

    /* Create the test file */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Close the file */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /*
     * Set up VFD SWMR configuration as writer in fapl1
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 10, presume ? true : false, true, true, false, true, 2, NULL,
                         MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* Open the file as VFD SWMR writer */
    if ((fid = H5Fopen(filename, H5F_ACC_RDWR, fapl1)) < 0)
        TEST_ERROR;

    /* Keep the file open */

    /* Open the same file again as VFD SWMR writer */
    /* Should succeed: 1st open--VFD SWMR writer, 2nd open--VFD SWMR writer */
    if ((fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl1)) < 0)
        TEST_ERROR;

    /* Close the second file open */
    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /*
     * Set up VFD SWMR configuration as reader in fapl2
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config2, 3, 8, presume ? true : false, false, true, false, true, 3, NULL,
                         MD_FILENAME, NULL);

    if ((fapl2 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl2, false, false, 4096, config2) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again as VFD SWMR reader */
    /* Should fail: 1st open--VFD SWMR writer, 2nd open--VFD SWMR reader */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDONLY, fapl2);
    }
    H5E_END_TRY;
    if (fid2 >= 0)
        TEST_ERROR;

    if (H5Pclose(fapl2) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again as regular writer */
    /* Should fail: 1st open--VFD SWMR writer, 2nd open--regular writer */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
    }
    H5E_END_TRY;
    if (fid2 >= 0)
        TEST_ERROR;

    /* Open the same file again as regular reader */
    /* Should fail: 1st open--VFD SWMR writer, 2nd open--regular reader */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
    }
    H5E_END_TRY;
    if (fid2 >= 0)
        TEST_ERROR;

    /* Close the 1st open file */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /*
     * Tests for second column
     *
     * For presume is false, only need to test for the 1st case
     */

    /*
     * Set up VFD SWMR configuration as reader in fapl1
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 10, presume ? true : false, false, true, false, true, 2, NULL,
                         MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* Open the file as VFD SWMR reader */
    /* Should fail because there is no metadata file */
    /* Take a while to complete due to retries */
    H5E_BEGIN_TRY
    {
        fid = H5Fopen(filename, H5F_ACC_RDONLY, fapl1);
    }
    H5E_END_TRY;

    if (!presume) {
        /* Should fail because there is no metadata file */
        /* Take a while to complete due to retries */
        if (fid >= 0)
            TEST_ERROR;
    }
    else { /* presume is true */
        /* Should succeed since presume_posix_semantics is true allowing make_believe data */
        if (fid < 0)
            TEST_ERROR;

        /* Continue testing for the remaining 3 cases */
        /* Keep the file open */

        /* config, tick_len, max_lag, presume_posix_semantics, writer,
         * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
         * md_file_path, md_file_name, updater_file_path */
        /* presume_posix_semantics is true */
        init_vfd_swmr_config(config2, 4, 10, true, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

        if ((fapl2 = H5Pcopy(orig_fapl)) < 0)
            FAIL_STACK_ERROR;

        /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
        if (vfd_swmr_fapl_augment(fapl2, false, false, 4096, config2) < 0)
            FAIL_STACK_ERROR;

        /* Open the same file again as VFD SWMR writer */
        /* Should fail */
        H5E_BEGIN_TRY
        {
            fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl2);
        }
        H5E_END_TRY;
        if (fid2 >= 0)
            TEST_ERROR;

        /* Open the same file again as VFD SWMR reader */
        /* Should succeed */
        if ((fid2 = H5Fopen(filename, H5F_ACC_RDONLY, fapl1)) < 0)
            TEST_ERROR;

        if (H5Fclose(fid2) < 0)
            FAIL_STACK_ERROR;

        /* Open the same file again as regular writer */
        /* Should fail: 1st open--VFD SWMR reader, 2nd open--regular writer */
        H5E_BEGIN_TRY
        {
            fid2 = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
        }
        H5E_END_TRY;
        if (fid2 >= 0)
            TEST_ERROR;

        /* Open the same file again as regular reader */
        /* Should fail: 1st open--VFD SWMR reader, 2nd open--regular reader */
        H5E_BEGIN_TRY
        {
            fid2 = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        }
        H5E_END_TRY;
        if (fid2 >= 0)
            TEST_ERROR;

        if (H5Fclose(fid) < 0)
            FAIL_STACK_ERROR;

        if (H5Pclose(fapl2) < 0)
            FAIL_STACK_ERROR;
    }

    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;

    /*
     * Tests for third column
     */

    /* Open the file as regular writer */
    if ((fid = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Keep the file open */

    /*
     * Set up as VFD SWMR configuration as writer in fapl1
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 10, presume ? true : false, true, true, false, true, 2, NULL,
                         MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again as VFD SWMR writer */
    /* Should fail: 1st open--regular writer, 2nd open--VFD SWMR writer */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl1);
    }
    H5E_END_TRY;
    if (fid2 >= 0)
        TEST_ERROR;

    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again as regular writer */
    /* Should succeed: 1st open--regular writer, 2nd open--regular writer */
    if ((fid2 = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again as regular reader */
    /* Should succeed: 1st open--regular writer, 2nd open--regular reader */
    if ((fid2 = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /* Close the 1st open file */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /*
     * Tests for fourth column
     */

    /* Open the file as regular reader */
    /* keep the file open */
    if ((fid = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /*
     * Set up VFD SWMR configuration as writer in fapl1
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 10, presume ? true : false, true, true, false, true, 2, NULL,
                         MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again as VFD SMWR writer */
    /* Should fail: 1st open--regular reader, 2nd open--VFD SWMR writer */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl1);
    }
    H5E_END_TRY;
    if (fid2 >= 0)
        TEST_ERROR;

    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again as regular reader */
    /* Should succeed: 1st open--regular reader, 2nd open--regular reader */
    if ((fid2 = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again as regular writer */
    /* Should fail: 1st open--regular reader, 2nd open--regular writer */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
    }
    H5E_END_TRY;
    if (fid2 >= 0)
        TEST_ERROR;

    /* Close the 1st open file */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config1);
    free(config2);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl1);
        H5Pclose(fapl2);
        H5Pclose(fcpl);
        H5Fclose(fid);
        H5Fclose(fid2);
    }
    H5E_END_TRY;

    free(config1);
    free(config2);

    return 1;
} /* test_same_file_opens() */

#ifndef _arraycount
#define _arraycount(_a) (sizeof(_a) / sizeof(_a[0]))
#endif

static unsigned
test_shadow_index_lookup(void)
{
    unsigned                   nerrors = 0;
    H5FD_vfd_swmr_idx_entry_t *idx;
    uint32_t                   size[] = {0, 1, 2, 3, 4, 0};
    unsigned                   seed   = 1;
    unsigned                   i, j, failj = UINT_MAX;
    hbool_t                    have_failj = false;
    unsigned long              tmpl;
    const char                *seedvar = "H5_SHADOW_INDEX_SEED";
    const char                *failvar = "H5_SHADOW_INDEX_FAIL";

    TESTING("Shadow-index lookups");

    /* get seed from environment or else from time(3) */
    switch (fetch_env_ulong(seedvar, UINT_MAX, &tmpl)) {
        case -1:
            nerrors = 1;
            goto out;
        case 0:
            seed = (unsigned int)time(NULL);
            break;
        default:
            seed = (unsigned int)tmpl;
            break;
    }

    /* get forced-fail index from environment */
    switch (fetch_env_ulong(failvar, UINT_MAX, &tmpl)) {
        case -1:
            nerrors = 1;
            goto out;
        case 0:
            break;
        default:
            failj      = (unsigned int)tmpl;
            have_failj = true;
            break;
    }

    srandom(seed);

    size[5] = (uint32_t)(1024 + random() % (16 * 1024 * 1024 - 1024));

    for (i = 0; i < _arraycount(size); i++) {
        uint32_t       cursize = size[i];
        const uint64_t modulus = UINT64_MAX / MAX(1, cursize);
        uint64_t       pageno;

        assert(modulus > 1); // so that modulus - 1 > 0, below

        idx = (cursize == 0) ? NULL : calloc(cursize, sizeof(*idx));
        if (idx == NULL && cursize != 0) {
            fprintf(stderr, "couldn't allocate %" PRIu32 " indices\n", cursize);
            exit(EXIT_FAILURE);
        }
        for (pageno = (uint64_t)random() % modulus, j = 0; j < cursize;
             j++, pageno += 1 + (uint64_t)random() % (modulus - 1)) {
            idx[j].hdf5_page_offset = pageno;
        }
        for (j = 0; j < cursize; j++) {
            H5FD_vfd_swmr_idx_entry_t *found;

            found = H5FD_vfd_swmr_pageno_to_mdf_idx_entry(idx, cursize, idx[j].hdf5_page_offset, false);
            if ((have_failj && failj == j) || found != &idx[j])
                break;
        }
        if (j < cursize) {
            printf("\nshadow-index entry %d lookup, pageno %" PRIu64 ", index size %" PRIu32 ", seed %u", j,
                   idx[j].hdf5_page_offset, cursize, seed);
            nerrors++;
        }
        if (idx != NULL)
            free(idx);
    }

out:
    if (nerrors == 0)
        PASSED();
    else
        printf(" FAILED\n");
    return nerrors;
}

/*-------------------------------------------------------------------------
 * Function:    test_enable_disable_eot()
 *
 * Purpose:     Verify the public routines:
 *                  H5Fvfd_swmr_enable_end_of_tick()
 *                  H5Fvfd_swmr_disable_end_of_tick()
 *               enables/disables EOT for the specified file
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; June 2020
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_enable_disable_eot(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN];     /* Filename to use */
    char                   filename2[FILE_NAME_LEN];    /* Filename to use */
    char                   filename3[FILE_NAME_LEN];    /* Filename to use */
    char                   non_filename[FILE_NAME_LEN]; /* Filename to use */
    hid_t                  fid   = H5I_INVALID_HID;     /* File ID */
    hid_t                  fid1  = H5I_INVALID_HID;     /* File ID */
    hid_t                  fid2  = H5I_INVALID_HID;     /* File ID */
    hid_t                  fid3  = H5I_INVALID_HID;     /* File ID */
    hid_t                  fcpl  = H5I_INVALID_HID;     /* File creation property list ID */
    hid_t                  fapl1 = H5I_INVALID_HID;     /* File access property list ID */
    hid_t                  fapl2 = H5I_INVALID_HID;     /* File access property list ID */
    hid_t                  fapl3 = H5I_INVALID_HID;     /* File access property list ID */
    H5F_t                 *f1, *f2, *f3;                /* File pointer */
    H5F_vfd_swmr_config_t *config1 = NULL;              /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *config2 = NULL;              /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *config3 = NULL;              /* Configuration for VFD SWMR */
    eot_queue_entry_t     *curr;                        /* Pointer to an entry on the EOT queue */
    unsigned               count = 0;                   /* Counter */
    herr_t                 ret;                         /* Return value */

    TESTING("H5Fvfd_swmr_enable/disable_end_of_tick()");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));
    h5_fixname(namebase2, orig_fapl, filename2, sizeof(filename));
    h5_fixname(namebase3, orig_fapl, filename3, sizeof(filename));
    h5_fixname(non_namebase, orig_fapl, non_filename, sizeof(non_filename));

    /* Allocate memory for the configuration structure */
    if ((config1 = malloc(sizeof(*config1))) == NULL)
        FAIL_STACK_ERROR;
    if ((config2 = malloc(sizeof(*config2))) == NULL)
        FAIL_STACK_ERROR;
    if ((config3 = malloc(sizeof(*config3))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Configured first file as VFD SWMR writer + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 6, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /*
     * Configured second file as VFD SWMR writer + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config2, 4, 6, false, true, true, false, true, 2, NULL, MD_FILENAME2, NULL);

    if ((fapl2 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl2, false, false, 4096, config2) < 0)
        FAIL_STACK_ERROR;

    /*
     * Configured third file as VFD SWMR writer + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config3, 4, 6, false, true, true, false, true, 2, NULL, MD_FILENAME3, NULL);

    if ((fapl3 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl3, false, false, 4096, config3) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create a file without VFD SWMR */
    if ((fid = H5Fcreate(non_filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Should fail to disable the file because VFD SWMR is not configured */
    H5E_BEGIN_TRY
    {
        ret = H5Fvfd_swmr_disable_end_of_tick(fid);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    if (H5Fclose(fid) < 0)
        TEST_ERROR;

    /* Create file 1 with VFD SWMR writer */
    if ((fid1 = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl1)) < 0)
        TEST_ERROR;

    /* Create file 2 with VFD SWMR writer */
    if ((fid2 = H5Fcreate(filename2, H5F_ACC_TRUNC, fcpl, fapl2)) < 0)
        TEST_ERROR;

    /* Create file 3 with VFD SWMR writer */
    if ((fid3 = H5Fcreate(filename3, H5F_ACC_TRUNC, fcpl, fapl3)) < 0)
        TEST_ERROR;

    /* Should have 3 files on the EOT queue */
    TAILQ_FOREACH(curr, &eot_queue_g, link)
    count++;
    if (count != 3)
        TEST_ERROR;

    /* Disable EOT for file 1 */
    if (H5Fvfd_swmr_disable_end_of_tick(fid1) < 0)
        TEST_ERROR;

    /* Disable file 1 again should fail because the file has just been disabled */
    H5E_BEGIN_TRY
    {
        ret = H5Fvfd_swmr_disable_end_of_tick(fid1);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Should have 2 files on the EOT queue */
    count = 0;
    TAILQ_FOREACH(curr, &eot_queue_g, link)
    count++;
    if (count != 2)
        TEST_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f1 = H5VL_object(fid1)))
        FAIL_STACK_ERROR;

    /* Should not find file 1 on the EOT queue */
    TAILQ_FOREACH(curr, &eot_queue_g, link)
    {
        if (curr->vfd_swmr_shared == f1->shared)
            break;
    }
    if (curr != NULL && curr->vfd_swmr_shared == f1->shared)
        TEST_ERROR;

    /* Enable EOT for file 2 should fail because the file has not been disabled */
    H5E_BEGIN_TRY
    {
        ret = H5Fvfd_swmr_enable_end_of_tick(fid2);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Get a pointer to the internal file object */
    if (NULL == (f2 = H5VL_object(fid2)))
        FAIL_STACK_ERROR;

    /* File 2 should be on the EOT queue */
    TAILQ_FOREACH(curr, &eot_queue_g, link)
    {
        if (curr->vfd_swmr_shared == f2->shared)
            break;
    }
    if (curr == NULL || curr->vfd_swmr_shared != f2->shared)
        TEST_ERROR;

    /* Close file 3 */
    if (H5Fclose(fid3) < 0)
        TEST_ERROR;

    /* Open file 3 again without VFD SWMR writer */
    if ((fid3 = H5Fopen(filename3, H5F_ACC_RDWR, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Get a pointer to the internal file object for file 3 */
    if (NULL == (f3 = H5VL_object(fid3)))
        FAIL_STACK_ERROR;

    /* File 3 should not exist on the EOT queue */
    TAILQ_FOREACH(curr, &eot_queue_g, link)
    {
        if (curr->vfd_swmr_shared == f3->shared)
            break;
    }
    if (curr != NULL && curr->vfd_swmr_shared == f3->shared)
        TEST_ERROR;

    /* Should have 2 files on the EOT queue */
    count = 0;
    TAILQ_FOREACH(curr, &eot_queue_g, link)
    count++;
    if (count != 1)
        TEST_ERROR;

    /* Should fail to enable file 3 */
    H5E_BEGIN_TRY
    {
        ret = H5Fvfd_swmr_enable_end_of_tick(fid3);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Should fail to disable file 3 */
    H5E_BEGIN_TRY
    {
        ret = H5Fvfd_swmr_disable_end_of_tick(fid3);
    }
    H5E_END_TRY;
    if (ret >= 0)
        TEST_ERROR;

    /* Closing */
    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;
    if (H5Fclose(fid3) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl1) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl2) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl3) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config1);
    free(config2);
    free(config3);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl1);
        H5Pclose(fapl2);
        H5Pclose(fapl3);
        H5Pclose(fcpl);
        H5Fclose(fid);
        H5Fclose(fid1);
        H5Fclose(fid2);
        H5Fclose(fid3);
    }
    H5E_END_TRY;

    free(config1);
    free(config2);
    free(config3);

    return 1;
} /* test_enable_disable_eot() */

/*-------------------------------------------------------------------------
 * Function:    verify_updater_flags()
 *
 * Purpose:     This is the helper routine used to verify whether
 *              "flags" in the updater file is as expected.
 *
 * Return:      SUCCEED/FAIL
 *
 * Programmer:  Vailin Choi; October 2021
 *
 *-------------------------------------------------------------------------
 */
static herr_t
verify_updater_flags(char *ud_name, uint16_t expected_flags)
{
    FILE    *ud_fp         = NULL;  /* Updater file pointer */
    uint16_t flags         = 0;     /* The "flags" field in the updater file */
    uint16_t swapped_flags = 0;     /* The "flags" field in the updater file */
    hbool_t  little_endian = false; /* Endianness of a machine */

    check_endian(&little_endian);

    /* Open the updater file */
    if ((ud_fp = fopen(ud_name, "r")) == NULL)
        FAIL_STACK_ERROR;

    /* Seek to the position of "flags" in the updater file's header */
    if (HDfseek(ud_fp, (HDoff_t)UD_HD_FLAGS_OFFSET, SEEK_SET) < 0)
        FAIL_STACK_ERROR;

    /* Read "flags" from the updater file */
    if (fread(&flags, UD_SIZE_2, 1, ud_fp) != (size_t)1)
        FAIL_STACK_ERROR;

    swapped_flags = little_endian ? flags : Swap2Bytes(flags);

    if (swapped_flags != expected_flags)
        TEST_ERROR;

    if (fclose(ud_fp) < 0)
        FAIL_STACK_ERROR;

    return SUCCEED;

error:
    return FAIL;

} /* verify_updater_flags() */

/*-------------------------------------------------------------------------
 * Function:    test_updater_flags
 *
 * Purpose:     Verify "flags" in the updater file is as expected for
 *              file creation.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2021
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_updater_flags(hid_t orig_fapl)
{
    char  filename[FILE_NAME_LEN];             /* Filename to use */
    hid_t fid       = H5I_INVALID_HID;         /* File ID */
    hid_t fcpl      = H5I_INVALID_HID;         /* File creation property list ID */
    hid_t fapl      = H5I_INVALID_HID;         /* File access property list ID */
    hid_t file_fapl = H5I_INVALID_HID;         /* File access property list ID associated with the file */
    H5F_vfd_swmr_config_t *config      = NULL; /* Configuration for VFD SWMR */
    H5F_vfd_swmr_config_t *file_config = NULL; /* Configuration for VFD SWMR */
    uint64_t               seq_num     = 0;    /* Sequence number for updater file */
    uint64_t               i           = 0;    /* Local index variable */
    char                   namebuf[H5F__MAX_VFD_SWMR_FILE_NAME_LEN]; /* Updater file path */
    h5_stat_t              sb;                                       /* Info returned by stat system call */

    TESTING("VFD SWMR updater file flags");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Should succeed without VFD SWMR configured */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Close the file  */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Allocate memory for the configuration structure */
    if ((config = (H5F_vfd_swmr_config_t *)malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;
    if ((file_config = (H5F_vfd_swmr_config_t *)malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Configured as VFD SWMR writer + page buffering + generate updater files
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 4, 7, false, true, true, true, true, 2, NULL, MD_FILENAME, UD_FILENAME);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, 4096, config) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl)) < 0)
        TEST_ERROR;

    /* Get the file's file access property list */
    if ((file_fapl = H5Fget_access_plist(fid)) < 0)
        FAIL_STACK_ERROR;

    /* Retrieve the VFD SWMR configuration from file_fapl */
    if (H5Pget_vfd_swmr_config(file_fapl, file_config) < 0)
        TEST_ERROR;

    /* Verify the retrieved info is the same as config1 */
    if (memcmp(config, file_config, sizeof(H5F_vfd_swmr_config_t)) != 0)
        TEST_ERROR;

    /* Verify the first updater file: "flags" field and file size */
    sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, seq_num);

    /* Verify "flags" of the first updater file */
    if (verify_updater_flags(namebuf, CREATE_METADATA_FILE_ONLY_FLAG) < 0)
        TEST_ERROR;

    /* Check updater file size */
    if (HDstat(namebuf, &sb) == 0 && sb.st_size < H5F_UD_HEADER_SIZE)
        TEST_ERROR;

    /* Closing */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Look for the last updater file */
    for (seq_num = 0;; seq_num++) {
        sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, seq_num);
        if (HDaccess(namebuf, F_OK) != 0)
            break;
    }
    sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, seq_num - 1);

    /* Verify "flags" of the last updater file */
    if (verify_updater_flags(namebuf, FINAL_UPDATE_FLAG) < 0)
        TEST_ERROR;

    if (H5Pclose(file_fapl) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Remove updater files */
    for (i = 0; i < seq_num; i++) {
        sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, i);
        HDremove(namebuf);
    }

    /* Free buffers */
    free(config);
    free(file_config);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
        H5Pclose(file_fapl);
        H5Pclose(fcpl);
        H5Fclose(fid);
    }
    H5E_END_TRY;

    free(config);
    free(file_config);

    return 1;
} /* test_updater_flags() */

/*-------------------------------------------------------------------------
 * Function:    test_updater_flags_same_file_opens()
 *
 * Purpose:     Verify "flags" in the updater file is as expected for
 *              multiple opens of the same file.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2021
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_updater_flags_same_file_opens(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN];                  /* Filename to use */
    hid_t                  fid     = H5I_INVALID_HID;                /* File ID */
    hid_t                  fid2    = H5I_INVALID_HID;                /* File ID */
    hid_t                  fcpl    = H5I_INVALID_HID;                /* File creation property list ID */
    hid_t                  fapl1   = H5I_INVALID_HID;                /* File access property list ID */
    H5F_vfd_swmr_config_t *config1 = NULL;                           /* Configuration for VFD SWMR */
    uint64_t               seq_num = 0;                              /* Sequence number for updater file */
    uint64_t               i       = 0;                              /* Local index variable */
    char                   namebuf[H5F__MAX_VFD_SWMR_FILE_NAME_LEN]; /* Updater file path */

    TESTING("VFD SWMR updater file flags for multiple opens of the same file");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Should succeed without VFD SWMR configured */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Close the file  */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Allocate memory for the configuration structure */
    if ((config1 = malloc(sizeof(*config1))) == NULL)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create the test file */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    /* Close the file */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /*
     * Set the VFD SWMR configuration in fapl1 + page buffering
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config1, 4, 10, false, true, true, true, true, 2, NULL, MD_FILENAME, UD_FILENAME);

    if ((fapl1 = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl1, false, false, 4096, config1) < 0)
        FAIL_STACK_ERROR;

    /* Open the file as VFD SWMR writer */
    /* Keep the file open */
    if ((fid = H5Fopen(filename, H5F_ACC_RDWR, fapl1)) < 0)
        TEST_ERROR;

    /* Open the same file again as VFD SWMR writer */
    /* Should succeed: 1st open--VFD SWMR writer, 2nd open--VFD SWMR writer */
    if ((fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl1)) < 0)
        TEST_ERROR;

    /* Close the second file open */
    if (H5Fclose(fid2) < 0)
        FAIL_STACK_ERROR;

    /* Verify the first updater file for first file open */
    sprintf(namebuf, "%s.%lu", UD_FILENAME, seq_num);

    /* Verify "flags" of the first updater file is 0*/
    if (verify_updater_flags(namebuf, 0) < 0)
        TEST_ERROR;

    /* Look for the last updater file */
    for (seq_num = 0;; seq_num++) {
        sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, seq_num);
        if (HDaccess(namebuf, F_OK) != 0)
            break;
    }
    sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, seq_num - 1);

    /* Verify "flags" of the last updater file is 0 */
    if (verify_updater_flags(namebuf, 0) < 0)
        TEST_ERROR;

    /* Close the 1st open file */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    /* Look for the last updater file */
    for (seq_num = 0;; seq_num++) {
        sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, seq_num);
        if (HDaccess(namebuf, F_OK) != 0)
            break;
    }
    sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, seq_num - 1);

    /* Verify "flags" of the last updater file after closing file */
    if (verify_updater_flags(namebuf, FINAL_UPDATE_FLAG) < 0)
        TEST_ERROR;

    /* Clean up updater files */
    for (i = 0; i < seq_num; i++) {
        sprintf(namebuf, "%s.%" PRIu64 "", UD_FILENAME, i);
        HDremove(namebuf);
    }

    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config1);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl1);
        H5Pclose(fcpl);
        H5Fclose(fid);
        H5Fclose(fid2);
    }
    H5E_END_TRY;

    free(config1);

    return 1;
} /* test_updater_flags_same_file_opens() */

/*-------------------------------------------------------------------------
 * Function:    clean_chk_ud_files()
 *
 * Purpose:     This is the helper routine used to clean up
 *              the checksum file and the updater files.
 *
 * Return:      void
 *
 * Programmer:  Vailin Choi; October 2021
 *
 *-------------------------------------------------------------------------
 */
static void
clean_chk_ud_files(char *md_file_path, char *updater_file_path)
{
    char     chk_name[FILE_NAME_LEN]; /* Checksum file name */
    char     ud_name[FILE_NAME_LEN];  /* Updater file name */
    uint64_t i;

    /* Name of the checksum file: <md_file_path>.chk */
    sprintf(chk_name, "%s.chk", md_file_path);

    /* Remove the checksum file if exists.
       If not, the callback will just continue appending
       checksums to the existing file */
    if (HDaccess(chk_name, F_OK) == 0) {
        HDremove(chk_name);
    }

    /* Remove all the updater files if exist: <updater_file_path>.<i> */
    for (i = 0;; i++) {
        sprintf(ud_name, "%s.%" PRIu64 "", updater_file_path, i);
        if (HDaccess(ud_name, F_OK) != 0)
            break;
        HDremove(ud_name);
    }

} /* clean_chk_ud_files() */

/*-------------------------------------------------------------------------
 * Function:    verify_ud_chk()
 *
 * Purpose:     This is the helper routine used by
 *              test_updater_generate_md_checksums() to verify
 *              contents of the checksum file and the updater files.
 *              --verify the sequence number in each updater's file header
 *                corresponds to the ith sequence number of the updater
 *                file name.
 *              --verify the tick number in each updater's file header
 *                corresponds to the tick number stored in the checksum file
 *              --verify the change_list_len in each updater's file header
 *                is consistent with num_change_list_entries in each updater's
 *                change list header
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2021
 *
 *-------------------------------------------------------------------------
 */
static herr_t
verify_ud_chk(char *md_file_path, char *ud_file_path)
{
    char     chk_name[FILE_NAME_LEN]; /* Checksum file name */
    char     ud_name[FILE_NAME_LEN];  /* Updater file name */
    FILE    *chk_fp = NULL;           /* Checksum file pointer */
    FILE    *ud_fp  = NULL;           /* Updater file pointer */
    uint64_t i;                       /* Local index variable */
    long     size = 0;                /* Size of the file */

    uint64_t chk_ud_seq_num = 0; /* Updater sequence number in the checksum file */

    uint64_t ud_seq_num              = 0; /* Sequence number in the updater file */
    uint64_t change_list_len         = 0; /* change_list_len in the updater file header */
    uint32_t num_change_list_entries = 0; /* num_change_list_entries in the updater change list header */

    uint64_t swapped_ud_seq_num              = 0;
    uint64_t swapped_change_list_len         = 0;
    uint32_t swapped_num_change_list_entries = 0;

    hbool_t little_endian = false;

    check_endian(&little_endian);

    /* Open the checksum file */
    sprintf(chk_name, "%s.chk", md_file_path);
    if ((chk_fp = fopen(chk_name, "r")) == NULL)
        FAIL_STACK_ERROR;

    for (i = 0;; i++) {
        /* Generate updater file name: <ud_file_path>.<i> */
        sprintf(ud_name, "%s.%" PRIu64 "", ud_file_path, i);

        /* Open the updater file */
        if ((ud_fp = fopen(ud_name, "r")) == NULL)
            break;
        else {
            /* Seek to the position of the sequence number in the updater file's header */
            if (HDfseek(ud_fp, (off_t)UD_HD_SEQ_NUM_OFFSET, SEEK_SET) < 0)
                FAIL_STACK_ERROR;

            /* Read the sequence number from the updater file */
            if (fread(&ud_seq_num, UD_SIZE_8, 1, ud_fp) != 1)
                FAIL_STACK_ERROR;

            swapped_ud_seq_num = little_endian ? ud_seq_num : Swap8Bytes(ud_seq_num);

            /* Compare the sequence number with i */
            if (swapped_ud_seq_num != i)
                TEST_ERROR;

            /* Read change_list_len from updater file's header */
            if (HDfseek(ud_fp, (off_t)UD_HD_CHANGE_LIST_LEN_OFFSET, SEEK_SET) < 0)
                FAIL_STACK_ERROR;

            if (fread(&change_list_len, UD_SIZE_8, 1, ud_fp) != 1)
                FAIL_STACK_ERROR;

            swapped_change_list_len = little_endian ? change_list_len : Swap8Bytes(change_list_len);

            if (i != 0) {

                /* Read num_change_list_entries from updater file's change list */
                if (HDfseek(ud_fp, (off_t)UD_CL_NUM_CHANGE_LIST_ENTRIES_OFFSET, SEEK_SET) < 0)
                    FAIL_STACK_ERROR;

                if (fread(&num_change_list_entries, UD_SIZE_4, 1, ud_fp) != 1)
                    FAIL_STACK_ERROR;

                swapped_num_change_list_entries =
                    little_endian ? num_change_list_entries : Swap4Bytes(num_change_list_entries);

                if (swapped_num_change_list_entries == 0) {
                    if (swapped_change_list_len != H5F_UD_CL_SIZE(0))
                        TEST_ERROR;
                }
                else {
                    if (swapped_change_list_len != H5F_UD_CL_SIZE(swapped_num_change_list_entries))
                        TEST_ERROR;
                }
            }

            /* Close the updater file */
            if (fclose(ud_fp) < 0)
                FAIL_STACK_ERROR;

            /* Read the updater sequence number from checksum file */
            if (fread(&chk_ud_seq_num, UD_SIZE_8, 1, chk_fp) != 1)
                FAIL_STACK_ERROR;

            /* Compare sequence number in updater file with sequence number in checksum file */
            if (swapped_ud_seq_num != chk_ud_seq_num)
                TEST_ERROR;

            /* Advance checksum file to the next sequence number */
            if (HDfseek(chk_fp, (off_t)UD_SIZE_4, SEEK_CUR) < 0)
                FAIL_STACK_ERROR;
        }
    }

    /* Get the size of the chksum file */
    if ((size = HDftell(chk_fp)) < 0)
        FAIL_STACK_ERROR;

    /* Size of sequence number and checksum in the checksum file */
    if ((unsigned)size != (i * (UD_SIZE_8 + UD_SIZE_4)))
        TEST_ERROR;

    return 0;

error:
    return -1;

} /* verify_ud_chk() */

/*-------------------------------------------------------------------------
 * Function:    md_ck_cb()
 *
 * Purpose:     This is the callback function used by
 *              test_updater_generate_md_checksums() when the
 *              H5F_ACS_GENERATE_MD_CK_CB_NAME property is set in fapl.
 *                  --Open and read the metadata file into a buffer.
 *                  --Generate checksum for the metadata file
 *                  --Write the tick number and the checksum to the checksum file
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2021
 *
 *-------------------------------------------------------------------------
 */
static herr_t
md_ck_cb(char *md_file_path, uint64_t updater_seq_num)
{
    FILE    *md_fp  = NULL;           /* Metadata file pointer */
    FILE    *chk_fp = NULL;           /* Checksum file pointer */
    long     size   = 0;              /* File size returned from HDftell() */
    void    *buf    = NULL;           /* Buffer for holding the metadata file content */
    uint32_t chksum = 0;              /* The checksum generated for the metadata file */
    char     chk_name[FILE_NAME_LEN]; /* Buffer for the checksum file name */
    size_t   ret;                     /* Return value */

    /* Open the metadata file */
    if ((md_fp = fopen(md_file_path, "r")) == NULL)
        FAIL_STACK_ERROR;

    /* Set file pointer at end of file.*/
    if (HDfseek(md_fp, 0, SEEK_END) < 0)
        FAIL_STACK_ERROR;

    /* Get the current position of the file pointer.*/
    if ((size = HDftell(md_fp)) < 0)
        FAIL_STACK_ERROR;

    if (size != 0) {

        rewind(md_fp);

        if ((buf = malloc((size_t)size)) == NULL)
            FAIL_STACK_ERROR;

        /* Read the metadata file to buf */
        if ((ret = fread(buf, 1, (size_t)size, md_fp)) != (size_t)size)
            FAIL_STACK_ERROR;

        /* Calculate checksum of the metadata file */
        chksum = H5_checksum_metadata(buf, (size_t)size, 0);
    }

    /* Close the metadata file */
    if (md_fp && fclose(md_fp) < 0)
        FAIL_STACK_ERROR;

    /*
     *  Checksum file
     */

    /* Generate checksum file name: <md_file_path>.chk */
    sprintf(chk_name, "%s.chk", md_file_path);

    /* Open checksum file for append */
    if ((chk_fp = fopen(chk_name, "a")) == NULL)
        FAIL_STACK_ERROR;

    /* Write the updater sequence number to the checksum file */
    if ((ret = fwrite(&updater_seq_num, sizeof(uint64_t), 1, chk_fp)) != 1)
        FAIL_STACK_ERROR;

    /* Write the checksum to the checksum file */
    if ((ret = fwrite(&chksum, sizeof(uint32_t), 1, chk_fp)) != 1)
        FAIL_STACK_ERROR;

    /* Close the checksum file */
    if (chk_fp && fclose(chk_fp) != 0)
        FAIL_STACK_ERROR;

    free(buf);

    return 0;

error:
    free(buf);

    if (md_fp)
        fclose(md_fp);
    if (chk_fp)
        fclose(chk_fp);

    return -1;
} /* md_ck_cb() */

/*-------------------------------------------------------------------------
 * Function:    test_updater_generate_md_checksums()
 *
 * Purpose:     It enables the generation of checksums for the metadata file
 *              created by the writer end of tick function.
 *              It also verifies the contents of the checksum file and the
 *              updater files.
 *
 *              The test is invoked when the file is created via H5Fcreate()
 *              and via H5Fopen().
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; October 2021
 *
 * Note: It is important to clean up the checksum file and the updater files.
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_updater_generate_md_checksums(hid_t orig_fapl, hbool_t file_create)
{
    char                    filename[FILE_NAME_LEN];  /* Filename to use */
    hid_t                   fid    = H5I_INVALID_HID; /* File ID */
    hid_t                   fcpl   = H5I_INVALID_HID; /* File creation property list ID */
    hid_t                   fapl   = H5I_INVALID_HID; /* File access property list ID */
    H5F_vfd_swmr_config_t  *config = NULL;            /* Configuration for VFD SWMR */
    H5F_generate_md_ck_cb_t cb_info;                  /* Callback */
    H5F_t                  *f                 = NULL; /* Internal file object pointer */
    char                   *md_file_path_name = NULL;

    if (file_create) {
        TESTING("VFD SWMR updater generate checksums for metadata file with H5Fcreate");
    }
    else {
        TESTING("VFD SWMR updater generate checksums for metadata file with H5Fopen");
    }

    h5_fixname(namebase4, orig_fapl, filename, sizeof(filename));

    if (NULL == (config = malloc(sizeof(H5F_vfd_swmr_config_t))))
        TEST_ERROR;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 4, 7, false, true, true, true, true, 2, NULL, MD_FILE, UD_FILE);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, true, false, 4096, config) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Set up callback to generate checksums for updater's metadata files */
    cb_info.func = md_ck_cb;

    /* Activate private property to generate checksums for updater's metadata file */
    if (H5Pset(fapl, H5F_ACS_GENERATE_MD_CK_CB_NAME, &cb_info) < 0)
        FAIL_STACK_ERROR;

    /* Use file creation or file open for testing */
    if (file_create) {
        if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl)) < 0)
            FAIL_STACK_ERROR;
    }
    else {
        if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, H5P_DEFAULT)) < 0)
            FAIL_STACK_ERROR;
        if (H5Fclose(fid) < 0)
            FAIL_STACK_ERROR;

        if ((fid = H5Fopen(filename, H5F_ACC_RDWR, fapl)) < 0)
            FAIL_STACK_ERROR;
    }

    /* Get a pointer to the internal file object */
    if (NULL == (f = (H5F_t *)H5VL_object(fid)))
        TEST_ERROR;

    /* Get the full metadata file pathname */
    md_file_path_name = strdup(f->shared->md_file_path_name);

    /* Close the file  */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;
    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Verify contents of checksum file and updater files */
    if (verify_ud_chk(md_file_path_name, config->updater_file_path) < 0)
        TEST_ERROR;

    /*  It's important to clean up the checksum and updater files. */
    clean_chk_ud_files(md_file_path_name, config->updater_file_path);

    free(config);

    PASSED();

    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
        H5Pclose(fcpl);
        H5Fclose(fid);
    }
    H5E_END_TRY;

    /*  It's important to clean up the checksum and updater files. */
    if (md_file_path_name && config)
        clean_chk_ud_files(md_file_path_name, config->updater_file_path);

    free(config);

    return 1;

} /* test_updater_generate_md_checksums() */

/*-------------------------------------------------------------------------
 * Function:    test_auto_generate_md()
 *
 * Purpose:     Verify the automatic generation of metadata filename
 *              when md_file_name is NULL.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; May 2022
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_auto_generate_md(hid_t orig_fapl, const char *md_path)
{
    char                   filename[FILE_NAME_LEN];    /* Filename to use */
    char                   newfilename[FILE_NAME_LEN]; /* Filename to use */
    hid_t                  fid  = H5I_INVALID_HID;     /* File ID */
    hid_t                  fcpl = H5I_INVALID_HID;     /* File creation property list ID */
    hid_t                  fapl = H5I_INVALID_HID; /* File access property list ID associated with the file */
    H5F_vfd_swmr_config_t *config = NULL;          /* Configuration for VFD SWMR */
    H5F_t                 *f;                      /* File pointer */

    TESTING("Automatic generation of metadata file name with/without md_path");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Allocate memory for the configuration structure */
    if ((config = (H5F_vfd_swmr_config_t *)malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Configured as VFD SWMR writer
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 4, 7, false, true, true, false, true, 2, md_path, NULL, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, 4096, config) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create the HDF5 file with VFD SWMR enabled */
    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl)) < 0)
        TEST_ERROR;

    /* Get internal file pointer */
    f = H5VL_object(fid);

    /* Zero out newfilename */
    memset(newfilename, 0, sizeof(newfilename));

    if (md_path != NULL) {
        strcat(newfilename, md_path);
        strcat(newfilename, "/");
    }

    strcat(newfilename, filename);
    strcat(newfilename, ".md");

    /* Compare the automatic generation of metadata filename is as expected */
    if (strcmp(f->shared->md_file_path_name, newfilename))
        TEST_ERROR;

    /* Closing */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fcpl);
        H5Pclose(fapl);
        H5Fclose(fid);
    }
    H5E_END_TRY;

    free(config);

    return 1;
} /* test_auto_generate_md() */

/*-------------------------------------------------------------------------
 * Function:    test_long_md_path_name()
 *
 * Purpose:     Verify failure when metadata pathname + filename
 *              that exceeds H5F__MAX_VFD_SWMR_FILE_NAME_LEN (1024)
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; May 2022
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_long_md_path_name(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN + 10];     /* Filename to use */
    char                   long_md_name[FILE_NAME_LEN + 10]; /* Filename to use */
    char                   long_md_path[FILE_NAME_LEN + 10]; /* Filename to use */
    hid_t                  fid  = H5I_INVALID_HID;           /* File ID */
    hid_t                  fapl = H5I_INVALID_HID; /* File access property list ID associated with the file */
    H5F_vfd_swmr_config_t *config = NULL;          /* Configuration for VFD SWMR */
    unsigned               i, times;
    int                    ret;

    TESTING("Generation of metadata pathname + filename exceeding H5F__MAX_VFD_SWMR_FILE_NAME_LEN");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Create long metadata pathname + filename that exceed H5F__MAX_VFD_SWMR_FILE_NAME_LEN */

    times = (FILE_NAME_LEN / 2) / strlen("long_md_name") + 1;

    memset(long_md_path, 0, sizeof(long_md_path));
    memset(long_md_name, 0, sizeof(long_md_name));

    for (i = 0; i < times; i++) {
        strcat(long_md_path, "long_md_path");
        strcat(long_md_name, "long_md_name");
    }

    /* Allocate memory for the configuration structure */
    if ((config = (H5F_vfd_swmr_config_t *)malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Configured as VFD SWMR writer
     */
    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 4, 7, false, true, true, false, true, 2, long_md_path, long_md_name, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    /* Should fail when calling H5Pset_vfd_swmr_config */
    H5E_BEGIN_TRY
    ret = vfd_swmr_fapl_augment(fapl, false, false, 4096, config);
    H5E_END_TRY;

    if (ret >= 0)
        TEST_ERROR;

    /* Free buffers */
    free(config);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
        H5Fclose(fid);
    }
    H5E_END_TRY;

    free(config);

    return 1;
} /* test_long_md_path_name() */

/*-------------------------------------------------------------------------
 * Function:    test_auto_long_md_path_name()
 *
 * Purpose:     Verify failure in automatic generation of metadata filename
 *              that exceeds H5F__MAX_VFD_SWMR_FILE_NAME_LEN (1024)
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 * Programmer:  Vailin Choi; May 2022
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_auto_long_md_path_name(hid_t orig_fapl)
{
    char                   filename[FILE_NAME_LEN + 10];      /* Filename to use */
    char                   long_namebase[FILE_NAME_LEN + 10]; /* Filename to use */
    hid_t                  fid  = H5I_INVALID_HID;            /* File ID */
    hid_t                  fcpl = H5I_INVALID_HID;            /* File creation property list ID */
    hid_t                  fapl = H5I_INVALID_HID; /* File access property list ID associated with the file */
    H5F_vfd_swmr_config_t *config = NULL;          /* Configuration for VFD SWMR */
    unsigned               i, times;

    TESTING("Automatic generation of metadata file name exceeding H5F__MAX_VFD_SWMR_FILE_NAME_LEN");

    /* Create long hdf5 filename to trigger automatic generation of long metadata filename */

    times = FILE_NAME_LEN / strlen(namebase) + 1;

    memset(long_namebase, 0, sizeof(long_namebase));

    /* Generate a long hdf5 filename that exceeds 1024 */
    for (i = 0; i < times; i++)
        strcat(long_namebase, namebase);

    h5_fixname(long_namebase, orig_fapl, filename, sizeof(filename));

    /*
     * Configured as VFD SWMR writer + NULL md_file_name
     */

    /* Allocate memory for the configuration structure */
    if ((config = (H5F_vfd_swmr_config_t *)malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        FAIL_STACK_ERROR;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 4, 7, false, true, true, false, true, 2, NULL, NULL, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, 4096, config) < 0)
        FAIL_STACK_ERROR;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    /* Create the HDF5 file with VFD SWMR enabled */
    /* Should fail when doing automatic generation of metadata filename
       because of the long hdf5 filename */
    H5E_BEGIN_TRY
    fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl);
    H5E_END_TRY;

    if (fid >= 0)
        TEST_ERROR;

    if (H5Pclose(fcpl) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fcpl);
        H5Pclose(fapl);
        H5Fclose(fid);
    }
    H5E_END_TRY;

    free(config);

    return 1;
} /* test_auto_long_md_path_name() */

/*-------------------------------------------------------------------------
 *  Function:    test_vfd_same_file_opens()
 *
 *  Purpose:     Verify multiple opens of the same file with different VFDs.
 *      Case #1:
 *          --Open a file with sec2 driver; the open succeeds
 *          --Open the same file again with stdio driver; this second
 *            open fails as expected due to "driver lock request failed".
 *          --Note: when the environment variable HDF5_USE_FILE_LOCKING is
 *            set to false, the open succeeds which shouldn't be; this is
 *            filed as a github issue.
 *
 *      Case #2:
 *          --Open a file with VFD SWMR configured; the open succeeds
 *          --Open the same file again with legacy SWMR; this second
 *            open fails as expected with "an already-open file conflicts
 *            with testfile.h5".
 *
 *      Case #3:
 *          --Open a file with legacy SWMR; the open succeeds
 *          --Open the same file again with VFD SWMR configured; this second
 *            open fails as expected.
 *
 *      Case #4:
 *          --Open a file as writer with both legacy SWMR and VFD SWMR configured .
 *          --The open should fail.
 *
 *      Case #5:
 *          --Open a file as reader with both legacy SWMR and VFD SWMR configured.
 *          --The open should fail.
 *
 *  Return:  0 if test is successful
 *           1 if test fails
 *
 *  Programmer:  Vailin Choi; May 2022
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_vfds_same_file_opens(hid_t orig_fapl, const char *env_h5_drvr)
{
    char                   filename[FILE_NAME_LEN];  /* Filename to use */
    hid_t                  fid1   = H5I_INVALID_HID; /* File ID */
    hid_t                  fid2   = H5I_INVALID_HID; /* File ID */
    hid_t                  fcpl   = H5I_INVALID_HID; /* File creation property list ID */
    hid_t                  fapl   = H5I_INVALID_HID; /* File access property list ID */
    H5F_vfd_swmr_config_t *config = NULL;            /* Configuration for VFD SWMR */

    TESTING("Multiple opens of the same file with different VFDs");

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    /* Set up file space strategy and file space page size in fcpl */
    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, 4096)) < 0) {
        printf("vfd_swmr_create_fcpl() failed");
        FAIL_STACK_ERROR;
    }

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* Set to latest format in fapl */
    if (H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST) < 0)
        FAIL_STACK_ERROR;

    /* Create the test file with latest format */
    if ((fid1 = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl)) < 0)
        TEST_ERROR;

    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;

    /* Close the file  */
    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;

    /*
     *  Case #1
     *  --Open the file with sec2 driver
     *  --Open the same file again with a different driver
     */

    /* The first open: with the default driver in H5P_DEFAULT */
    if ((fid1 = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT)) < 0)
        TEST_ERROR;

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    if (H5Pset_fapl_stdio(fapl) < 0)
        FAIL_STACK_ERROR;

    /* Open the same file again with stdio driver */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl);
    }
    H5E_END_TRY;
    /* This is for check-vfd: the open will succeed
       if the HDF5_DRIVER environment variable is set to "stdio" */
    if (strcmp(env_h5_drvr, "stdio") == 0) {
        if (fid2 < 0)
            TEST_ERROR;
        if (H5Fclose(fid2) < 0)
            FAIL_STACK_ERROR;
    }
    else {
        /* Should fail: due to "driver lock request failed" */
        if (fid2 >= 0)
            TEST_ERROR;
    }

    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;

    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;

    /*
     *  Case #2
     *  --Open the file with VFD SWMR configured
     *  --Open the same file again with legacy SWMR
     */

    /* Allocate memory for the configuration structure */
    if ((config = malloc(sizeof(*config))) == NULL)
        FAIL_STACK_ERROR;

    /*
     * Set up VFD SWMR configuration as writer in fapl
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 4, 10, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, 4096, config) < 0)
        FAIL_STACK_ERROR;

    /* The first open: as VFD SWMR writer */
    if ((fid1 = H5Fopen(filename, H5F_ACC_RDWR, fapl)) < 0)
        TEST_ERROR;

    /* The second open: as legacy SWMR writer */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDWR | H5F_ACC_SWMR_WRITE, H5P_DEFAULT);
    }
    H5E_END_TRY;
    /* Should fail */
    if (fid2 >= 0)
        TEST_ERROR;

    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;

    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;

    /* Test cases #3 - #5 involve legacy SWMR.  Therefore tests are
       skipped if driver does not support the feature */
    if (!H5FD__supports_swmr_test(env_h5_drvr)) {
        printf("The %s driver does not support legacy SWMR.\n", env_h5_drvr);
        printf("Test cases #3 - #5 for this test are skipped.\n");
        PASSED();
        return 0;
    }

    /*
     *  Case #3
     *  --Open the file with legacy SWMR
     *  --Open the same file again with VFD SWMR configured
     */

    /* The first open: as writer with legacy SWMR */
    if ((fid1 = H5Fopen(filename, H5F_ACC_RDWR | H5F_ACC_SWMR_WRITE, H5P_DEFAULT)) < 0)
        FAIL_STACK_ERROR;

    /*
     * Set up VFD SWMR configuration as writer in fapl
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 4, 10, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, 4096, config) < 0)
        FAIL_STACK_ERROR;

    /* The second open: as writer with VFD SWMR */
    H5E_BEGIN_TRY
    {
        fid2 = H5Fopen(filename, H5F_ACC_RDWR, fapl);
    }
    H5E_END_TRY;
    /* Should fail */
    if (fid2 >= 0)
        TEST_ERROR;

    if (H5Fclose(fid1) < 0)
        FAIL_STACK_ERROR;

    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;

    /*
     *  Case #4
     *  --Open the file as writer with both legacy SWMR and VFD SWMR configured .
     */

    /*
     * Set up VFD SWMR configuration as writer in fapl
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 4, 10, false, true, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, 4096, config) < 0)
        FAIL_STACK_ERROR;

    /* Should fail */
    H5E_BEGIN_TRY
    {
        fid1 = H5Fopen(filename, H5F_ACC_RDWR | H5F_ACC_SWMR_WRITE, fapl);
    }
    H5E_END_TRY;
    if (fid1 >= 0) {
        if (H5Fclose(fid1) < 0)
            FAIL_STACK_ERROR;
        TEST_ERROR;
    }

    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;

    /*
     *  Case #5:
     *  --Open the file as reader with both legacy SWMR and VFD SWMR configured .
     */

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    /* NOTE: Set "presume_posix_semantics" to true and "writer" to false */
    init_vfd_swmr_config(config, 4, 10, true, false, true, false, true, 2, NULL, MD_FILENAME, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        FAIL_STACK_ERROR;

    /* fapl, use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, 4096, config) < 0)
        FAIL_STACK_ERROR;

    /* Should fail */
    H5E_BEGIN_TRY
    {
        fid1 = H5Fopen(filename, H5F_ACC_RDONLY | H5F_ACC_SWMR_READ, fapl);
    }
    H5E_END_TRY;
    if (fid1 >= 0) {
        if (H5Fclose(fid1) < 0)
            FAIL_STACK_ERROR;
        TEST_ERROR;
    }

    if (H5Pclose(fapl) < 0)
        FAIL_STACK_ERROR;

    /* Free buffers */
    free(config);

    PASSED();
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
        H5Pclose(fcpl);
        H5Fclose(fid1);
        H5Fclose(fid2);
    }
    H5E_END_TRY;

    free(config);

    return 1;
} /* test_vfds_same_file_opens() */

/*-------------------------------------------------------------------------
 * Function:    md_fsm_open_writer()
 *
 * Purpose:     Create a VFD SWMR writer file and hand back both the file ID
 *              and the internal file object, so that the free-space manager
 *              for the metadata (shadow) file can be driven directly through
 *              the H5MV interface.
 *
 * Return:      0 if successful
 *              1 if it fails
 *
 *-------------------------------------------------------------------------
 */
static int
md_fsm_open_writer(hid_t orig_fapl, hid_t *fidp, H5F_t **fp)
{
    char                   filename[FILE_NAME_LEN];  /* Filename to use */
    hid_t                  fapl   = H5I_INVALID_HID; /* File access property list */
    hid_t                  fcpl   = H5I_INVALID_HID; /* File creation property list */
    hid_t                  fid    = H5I_INVALID_HID; /* File ID */
    H5F_vfd_swmr_config_t *config = NULL;            /* Configuration for VFD SWMR */

    *fidp = H5I_INVALID_HID;
    *fp   = NULL;

    h5_fixname(namebase, orig_fapl, filename, sizeof(filename));

    if ((config = malloc(sizeof(H5F_vfd_swmr_config_t))) == NULL)
        goto error;

    /* config, tick_len, max_lag, presume_posix_semantics, writer,
     * maintain_metadata_file, generate_updater_files, flush_raw_data, md_pages_reserved,
     * md_file_path, md_file_name, updater_file_path */
    init_vfd_swmr_config(config, 1, 3, false, true, true, false, true, 256, NULL, MD_FILENAME, NULL);

    if ((fapl = H5Pcopy(orig_fapl)) < 0)
        goto error;

    /* use_latest_format, only_meta_page, page_buf_size, config */
    if (vfd_swmr_fapl_augment(fapl, false, false, FS_PAGE_SIZE, config) < 0)
        goto error;

    if ((fcpl = vfd_swmr_create_fcpl(H5F_FSPACE_STRATEGY_PAGE, FS_PAGE_SIZE)) < 0)
        goto error;

    if ((fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl, fapl)) < 0)
        goto error;

    /* Get a pointer to the internal file object */
    if (NULL == (*fp = (H5F_t *)H5VL_object(fid)))
        goto error;

    if (H5Pclose(fapl) < 0)
        goto error;
    if (H5Pclose(fcpl) < 0)
        goto error;

    free(config);

    *fidp = fid;

    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
        H5Pclose(fcpl);
        H5Fclose(fid);
    }
    H5E_END_TRY;

    free(config);

    *fp = NULL;

    return 1;
} /* md_fsm_open_writer() */

/*-------------------------------------------------------------------------
 * Function:    md_fsm_reset()
 *
 * Purpose:     Put the metadata file's allocator into a state the caller can
 *              reason about: no free-space manager and a page-aligned EOA.
 *
 *              H5MV_close() drops whatever sections the writer left behind
 *              (that space is simply leaked in the shadow file, which is
 *              harmless here), and allocating one page across the alignment
 *              gap walks the EOA up to a page boundary.  Closing a second
 *              time then discards the fragment that allocation created.
 *
 *              The resulting EOA is returned through *eoap.
 *
 * Return:      0 if successful
 *              1 if it fails
 *
 *-------------------------------------------------------------------------
 */
static int
md_fsm_reset(H5F_t *f, haddr_t *eoap)
{
    H5F_shared_t *shared = f->shared;
    haddr_t       eoa;

    if (H5MV_close(f) < 0)
        return 1;

    eoa = H5MV_get_vfd_swmr_md_eoa(shared);

    if (eoa % FS_PAGE_SIZE != 0) {
        if (HADDR_UNDEF == H5MV_alloc(f, FS_PAGE_SIZE))
            return 1;
        if (H5MV_close(f) < 0)
            return 1;
        eoa = H5MV_get_vfd_swmr_md_eoa(shared);
    }

    if (eoa % FS_PAGE_SIZE != 0 || shared->fs_man_md != NULL)
        return 1;

    *eoap = eoa;

    return 0;
} /* md_fsm_reset() */

/*-------------------------------------------------------------------------
 * Function:    test_md_alloc_free()
 *
 * Purpose:     Verify H5MV_alloc()/H5MV_free() for the metadata file:
 *              --a request that does not start on a page boundary is aligned
 *                and the leading fragment becomes reusable free space
 *              --freeing a block away from the EOA starts the free-space
 *                manager and leaves the EOA alone
 *              --a freed block is handed back to the next matching request
 *              --adjacent freed blocks merge into a single section
 *              --a request smaller than a section splits it
 *              --freeing an undefined address or a zero-length block is a
 *                no-op
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_md_alloc_free(hid_t orig_fapl)
{
    hid_t         fid    = H5I_INVALID_HID; /* File ID */
    H5F_t        *f      = NULL;            /* Internal file object pointer */
    H5F_shared_t *shared = NULL;            /* Shared file object pointer */
    haddr_t       eoa_before;               /* EOA before an operation */
    hsize_t       frag;                     /* Expected alignment fragment size */
    haddr_t       base;                     /* Page-aligned EOA to allocate from */
    haddr_t       a1, a2, a3, b, c1, c2;    /* Blocks allocated from the metadata file */

    /* Size of a sub-page allocation, used to knock the EOA off a page boundary */
    const hsize_t skew = 100;

    TESTING("H5MV alloc/free for the VFD SWMR metadata file");

    if (md_fsm_open_writer(orig_fapl, &fid, &f) != 0)
        FAIL_STACK_ERROR;
    shared = f->shared;

    if (md_fsm_reset(f, &base) != 0)
        FAIL_STACK_ERROR;

    /* (1) A sub-page request is served from the EOA as-is, leaving the EOA off
     * a page boundary.
     */
    if (HADDR_UNDEF == (a1 = H5MV_alloc(f, skew)))
        FAIL_STACK_ERROR;

    if (a1 != base) {
        printf("Expected an allocation at %" PRIuHADDR " but got %" PRIuHADDR "\n", base, a1);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != base + skew) {
        printf("EOA is not immediately past the allocated block\n");
        TEST_ERROR;
    }
    if (shared->fs_man_md != NULL) {
        printf("An aligned allocation from the EOA should not start a free-space manager\n");
        TEST_ERROR;
    }

    /* The next request is therefore aligned past the EOA, and the bytes it
     * skipped become a free-space section.
     */
    frag = FS_PAGE_SIZE - skew;

    if (HADDR_UNDEF == (a2 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (a2 != base + FS_PAGE_SIZE) {
        printf("Expected an aligned allocation at %" PRIuHADDR " but got %" PRIuHADDR "\n",
               base + FS_PAGE_SIZE, a2);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != a2 + FS_PAGE_SIZE) {
        printf("EOA is not immediately past the allocated block\n");
        TEST_ERROR;
    }
    if (shared->fs_man_md == NULL) {
        printf("The skipped bytes should have started a free-space manager\n");
        TEST_ERROR;
    }

    /* The fragment is reusable */
    eoa_before = H5MV_get_vfd_swmr_md_eoa(shared);

    if (HADDR_UNDEF == (b = H5MV_alloc(f, frag)))
        FAIL_STACK_ERROR;
    if (b != base + skew) {
        printf("The alignment fragment at %" PRIuHADDR " was not reused; got %" PRIuHADDR "\n", base + skew,
               b);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("Reusing the alignment fragment should not move the EOA\n");
        TEST_ERROR;
    }

    /* (2) Three consecutive page-sized blocks come back contiguously */
    if (md_fsm_reset(f, &base) != 0)
        FAIL_STACK_ERROR;

    if (HADDR_UNDEF == (a1 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a2 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a3 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (a1 != base || a2 != base + FS_PAGE_SIZE || a3 != base + 2 * FS_PAGE_SIZE) {
        printf("Page-sized allocations from an aligned EOA are not contiguous\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != base + 3 * FS_PAGE_SIZE) {
        printf("EOA is not immediately past the three allocated blocks\n");
        TEST_ERROR;
    }
    if (shared->fs_man_md != NULL) {
        printf("Aligned allocations from the EOA should not start a free-space manager\n");
        TEST_ERROR;
    }

    /* (3) Freeing the middle block starts the manager and leaves the EOA put */
    if (H5MV_free(f, a2, FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;

    if (shared->fs_man_md == NULL) {
        printf("Freeing a block away from the EOA should start the free-space manager\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != base + 3 * FS_PAGE_SIZE) {
        printf("Freeing a block away from the EOA should not move the EOA\n");
        TEST_ERROR;
    }

    /* (4) The freed block satisfies the next request of the same size */
    if (HADDR_UNDEF == (b = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (b != a2) {
        printf("Expected the freed block at %" PRIuHADDR " to be reused; got %" PRIuHADDR "\n", a2, b);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != base + 3 * FS_PAGE_SIZE) {
        printf("Reusing a freed block should not move the EOA\n");
        TEST_ERROR;
    }

    /* (5) Adjacent freed blocks merge: a two-page request is satisfied from
     * them instead of growing the file.
     */
    if (H5MV_free(f, a1, FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;
    if (H5MV_free(f, a2, FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;

    if (HADDR_UNDEF == (b = H5MV_alloc(f, 2 * FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (b != a1) {
        printf("Expected the merged section at %" PRIuHADDR " to be reused; got %" PRIuHADDR "\n", a1, b);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != base + 3 * FS_PAGE_SIZE) {
        printf("Two adjacent freed pages should have merged instead of growing the file\n");
        TEST_ERROR;
    }

    /* (6) A request smaller than the section splits it; the remainder stays
     * available.
     */
    if (H5MV_free(f, b, 2 * FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;

    if (HADDR_UNDEF == (c1 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (c2 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (c1 != a1 || c2 != a1 + FS_PAGE_SIZE) {
        printf("A two-page section did not split into two page-sized allocations\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != base + 3 * FS_PAGE_SIZE) {
        printf("Splitting a section should not move the EOA\n");
        TEST_ERROR;
    }

    /* (7) Freeing nothing does nothing */
    eoa_before = H5MV_get_vfd_swmr_md_eoa(shared);

    if (H5MV_free(f, HADDR_UNDEF, FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;
    if (H5MV_free(f, a3, 0) < 0)
        FAIL_STACK_ERROR;

    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("Freeing an undefined address or a zero-length block should not move the EOA\n");
        TEST_ERROR;
    }

    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    PASSED();

    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Fclose(fid);
    }
    H5E_END_TRY;

    return 1;
} /* test_md_alloc_free() */

/*-------------------------------------------------------------------------
 * Function:    test_md_try_extend()
 *
 * Purpose:     Verify H5MV_try_extend() for the metadata file:
 *              --a block at the EOA is extended by pushing the EOA out
 *              --a block that is neither at the EOA nor followed by free
 *                space cannot be extended
 *              --a block followed by a large enough free-space section is
 *                extended into that section, consuming it
 *              --a section larger than the request is only partly consumed
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_md_try_extend(hid_t orig_fapl)
{
    hid_t         fid    = H5I_INVALID_HID; /* File ID */
    H5F_t        *f      = NULL;            /* Internal file object pointer */
    H5F_shared_t *shared = NULL;            /* Shared file object pointer */
    haddr_t       base;                     /* Page-aligned EOA to allocate from */
    haddr_t       eoa_before;               /* EOA before an operation */
    haddr_t       a1, a2, a3, b;            /* Blocks allocated from the metadata file */
    htri_t        extended;                 /* Whether the block was extended */

    TESTING("H5MV_try_extend() for the VFD SWMR metadata file");

    if (md_fsm_open_writer(orig_fapl, &fid, &f) != 0)
        FAIL_STACK_ERROR;
    shared = f->shared;

    if (md_fsm_reset(f, &base) != 0)
        FAIL_STACK_ERROR;

    /* (1) A block at the EOA is extended by moving the EOA */
    if (HADDR_UNDEF == (a1 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if ((extended = H5MV_try_extend(f, a1, FS_PAGE_SIZE, FS_PAGE_SIZE)) < 0)
        FAIL_STACK_ERROR;
    if (!extended) {
        printf("A block at the EOA should have been extended\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != a1 + 2 * FS_PAGE_SIZE) {
        printf("Extending a block at the EOA should have moved the EOA out by one page\n");
        TEST_ERROR;
    }

    /* (2) A block with allocated space after it cannot be extended */
    if (HADDR_UNDEF == (a2 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a3 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (a2 != a1 + 2 * FS_PAGE_SIZE || a3 != a2 + FS_PAGE_SIZE) {
        printf("Page-sized allocations from an aligned EOA are not contiguous\n");
        TEST_ERROR;
    }

    eoa_before = H5MV_get_vfd_swmr_md_eoa(shared);

    if ((extended = H5MV_try_extend(f, a1, FS_PAGE_SIZE, FS_PAGE_SIZE)) < 0)
        FAIL_STACK_ERROR;
    if (extended) {
        printf("A block followed by allocated space should not have been extended\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("A failed extension should not move the EOA\n");
        TEST_ERROR;
    }

    /* (3) A block followed by a free-space section extends into it */
    if (H5MV_free(f, a2, FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;

    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("Freeing a block away from the EOA should not move the EOA\n");
        TEST_ERROR;
    }

    if ((extended = H5MV_try_extend(f, a1, 2 * FS_PAGE_SIZE, FS_PAGE_SIZE)) < 0)
        FAIL_STACK_ERROR;
    if (!extended) {
        printf("A block followed by a free-space section should have been extended\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("Extending into a free-space section should not move the EOA\n");
        TEST_ERROR;
    }

    /* The section was consumed, so the next request has to grow the file */
    if (HADDR_UNDEF == (b = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (b != eoa_before) {
        printf("Expected an allocation at the EOA (%" PRIuHADDR ") but got %" PRIuHADDR "\n", eoa_before, b);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before + FS_PAGE_SIZE) {
        printf("EOA is not immediately past the allocated block\n");
        TEST_ERROR;
    }

    /* (4) Extending by less than a section leaves the remainder behind */
    if (md_fsm_reset(f, &base) != 0)
        FAIL_STACK_ERROR;

    if (HADDR_UNDEF == (a1 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a2 = H5MV_alloc(f, 2 * FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a3 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    eoa_before = H5MV_get_vfd_swmr_md_eoa(shared);

    if (H5MV_free(f, a2, 2 * FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;

    if ((extended = H5MV_try_extend(f, a1, FS_PAGE_SIZE, FS_PAGE_SIZE)) < 0)
        FAIL_STACK_ERROR;
    if (!extended) {
        printf("A block followed by a two-page section should have been extended\n");
        TEST_ERROR;
    }

    /* One page of the two-page section is left, at its far end */
    if (HADDR_UNDEF == (b = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (b != a2 + FS_PAGE_SIZE) {
        printf("Expected the remainder at %" PRIuHADDR " to be reused; got %" PRIuHADDR "\n",
               a2 + FS_PAGE_SIZE, b);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("Reusing the remainder of a partly consumed section should not move the EOA\n");
        TEST_ERROR;
    }

    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    PASSED();

    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Fclose(fid);
    }
    H5E_END_TRY;

    return 1;
} /* test_md_try_extend() */

/*-------------------------------------------------------------------------
 * Function:    test_md_try_shrink()
 *
 * Purpose:     Verify H5MV_try_shrink() and the shrink path of H5MV_free()
 *              for the metadata file:
 *              --a block away from the EOA cannot shrink the file
 *              --a block at the EOA pulls the EOA back
 *              --H5MV_free() of a block at the EOA shrinks the file rather
 *                than starting a free-space manager
 *              --with a manager in play, freeing at the EOA still shrinks the
 *                file, absorbing the section that adjoins it
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_md_try_shrink(hid_t orig_fapl)
{
    hid_t         fid    = H5I_INVALID_HID; /* File ID */
    H5F_t        *f      = NULL;            /* Internal file object pointer */
    H5F_shared_t *shared = NULL;            /* Shared file object pointer */
    haddr_t       base;                     /* Page-aligned EOA to allocate from */
    haddr_t       a1, a2, a3, b;            /* Blocks allocated from the metadata file */
    htri_t        shrunk;                   /* Whether the file was shrunk */

    TESTING("H5MV_try_shrink() for the VFD SWMR metadata file");

    if (md_fsm_open_writer(orig_fapl, &fid, &f) != 0)
        FAIL_STACK_ERROR;
    shared = f->shared;

    if (md_fsm_reset(f, &base) != 0)
        FAIL_STACK_ERROR;

    if (HADDR_UNDEF == (a1 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a2 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    /* (1) A block away from the EOA cannot shrink the file */
    if ((shrunk = H5MV_try_shrink(f, a1, FS_PAGE_SIZE)) < 0)
        FAIL_STACK_ERROR;
    if (shrunk) {
        printf("A block away from the EOA should not have shrunk the file\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != base + 2 * FS_PAGE_SIZE) {
        printf("A failed shrink should not move the EOA\n");
        TEST_ERROR;
    }

    /* (2) A block at the EOA pulls the EOA back, twice in a row */
    if ((shrunk = H5MV_try_shrink(f, a2, FS_PAGE_SIZE)) < 0)
        FAIL_STACK_ERROR;
    if (!shrunk) {
        printf("A block at the EOA should have shrunk the file\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != a2) {
        printf("Shrinking should have pulled the EOA back to %" PRIuHADDR "\n", a2);
        TEST_ERROR;
    }

    if ((shrunk = H5MV_try_shrink(f, a1, FS_PAGE_SIZE)) < 0)
        FAIL_STACK_ERROR;
    if (!shrunk) {
        printf("The block now at the EOA should have shrunk the file\n");
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != base) {
        printf("Shrinking should have pulled the EOA back to %" PRIuHADDR "\n", base);
        TEST_ERROR;
    }

    /* (3) H5MV_free() of a block at the EOA shrinks the file without starting
     * a free-space manager
     */
    if (HADDR_UNDEF == (a1 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a2 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (H5MV_free(f, a2, FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;

    if (H5MV_get_vfd_swmr_md_eoa(shared) != a2) {
        printf("Freeing the block at the EOA should have shrunk the file\n");
        TEST_ERROR;
    }
    if (shared->fs_man_md != NULL) {
        printf("Freeing the block at the EOA should not have started a free-space manager\n");
        TEST_ERROR;
    }

    /* (4) With a manager in play, freeing at the EOA absorbs the adjoining
     * section as well
     */
    if (HADDR_UNDEF == (a2 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a3 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (H5MV_free(f, a2, FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;

    if (shared->fs_man_md == NULL) {
        printf("Freeing a block away from the EOA should have started a free-space manager\n");
        TEST_ERROR;
    }

    if (H5MV_free(f, a3, FS_PAGE_SIZE) < 0)
        FAIL_STACK_ERROR;

    if (H5MV_get_vfd_swmr_md_eoa(shared) != a2) {
        printf("Freeing at the EOA should have shrunk the file past the adjoining section\n");
        TEST_ERROR;
    }

    /* The absorbed section is gone, so the next request comes from the EOA */
    if (HADDR_UNDEF == (b = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (b != a2) {
        printf("Expected an allocation at the EOA (%" PRIuHADDR ") but got %" PRIuHADDR "\n", a2, b);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != a2 + FS_PAGE_SIZE) {
        printf("EOA is not immediately past the allocated block\n");
        TEST_ERROR;
    }

    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    PASSED();

    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Fclose(fid);
    }
    H5E_END_TRY;

    return 1;
} /* test_md_try_shrink() */

/*-------------------------------------------------------------------------
 * Function:    test_md_sect_alignment()
 *
 * Purpose:     Verify that the metadata file's free-space manager honors page
 *              alignment when it reuses a section: a request of at least one
 *              page takes an aligned block out of the middle of a misaligned
 *              section, and the leading fragment the split leaves behind is
 *              still usable by a smaller request.  This drives the
 *              H5MV__sect_split() callback.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_md_sect_alignment(hid_t orig_fapl)
{
    hid_t         fid    = H5I_INVALID_HID; /* File ID */
    H5F_t        *f      = NULL;            /* Internal file object pointer */
    H5F_shared_t *shared = NULL;            /* Shared file object pointer */
    haddr_t       base;                     /* Page-aligned EOA to allocate from */
    haddr_t       eoa_before;               /* EOA before an operation */
    haddr_t       a1, a2, a3, b, c;         /* Blocks allocated from the metadata file */

    /* Offset into a page at which the misaligned section starts */
    const hsize_t skew = 100;

    TESTING("H5MV free-space section alignment for the VFD SWMR metadata file");

    if (md_fsm_open_writer(orig_fapl, &fid, &f) != 0)
        FAIL_STACK_ERROR;
    shared = f->shared;

    if (md_fsm_reset(f, &base) != 0)
        FAIL_STACK_ERROR;

    /* a3 keeps the freed region below the EOA, so that freeing it creates a
     * section instead of shrinking the file
     */
    if (HADDR_UNDEF == (a1 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a2 = H5MV_alloc(f, 4 * FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;
    if (HADDR_UNDEF == (a3 = H5MV_alloc(f, FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (a1 != base || a2 != base + FS_PAGE_SIZE || a3 != base + 5 * FS_PAGE_SIZE) {
        printf("Allocations from an aligned EOA are not contiguous\n");
        TEST_ERROR;
    }

    eoa_before = H5MV_get_vfd_swmr_md_eoa(shared);

    /* Free a range that starts part way into a page */
    if (H5MV_free(f, a2 + skew, 4 * FS_PAGE_SIZE - skew) < 0)
        FAIL_STACK_ERROR;

    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("Freeing a block away from the EOA should not move the EOA\n");
        TEST_ERROR;
    }

    /* A two-page request is served from the aligned interior of that section */
    if (HADDR_UNDEF == (b = H5MV_alloc(f, 2 * FS_PAGE_SIZE)))
        FAIL_STACK_ERROR;

    if (b % FS_PAGE_SIZE != 0) {
        printf("A page-sized request returned the misaligned address %" PRIuHADDR "\n", b);
        TEST_ERROR;
    }
    if (b != a2 + FS_PAGE_SIZE) {
        printf("Expected an allocation at %" PRIuHADDR " but got %" PRIuHADDR "\n", a2 + FS_PAGE_SIZE, b);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("Reusing a section should not move the EOA\n");
        TEST_ERROR;
    }

    /* The leading fragment the split left behind is still usable, and a
     * request smaller than a page is not aligned
     */
    if (HADDR_UNDEF == (c = H5MV_alloc(f, skew)))
        FAIL_STACK_ERROR;

    if (c != a2 + skew) {
        printf("Expected the leading fragment at %" PRIuHADDR " to be reused; got %" PRIuHADDR "\n",
               a2 + skew, c);
        TEST_ERROR;
    }
    if (H5MV_get_vfd_swmr_md_eoa(shared) != eoa_before) {
        printf("Reusing the leading fragment should not move the EOA\n");
        TEST_ERROR;
    }

    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    PASSED();

    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Fclose(fid);
    }
    H5E_END_TRY;

    return 1;
} /* test_md_sect_alignment() */

/*-------------------------------------------------------------------------
 * Function:    test_mpmde_read_across_page()
 *
 * Purpose:     Regression test for the H5PB_read() overflow clamp on
 *              multi-page metadata entries.
 *
 *              A VFD SWMR writer stores a metadata write of at least one
 *              page as a single multi-page entry (is_mpmde), whose real
 *              size can be many pages. H5PB_read()'s "found" branch clamped
 *              the copy to page_buf->page_size rather than to the entry's
 *              own size, so a read that started inside the entry's first
 *              page and continued past the page boundary silently returned
 *              only the bytes up to that boundary -- the caller's buffer
 *              kept whatever it already held for the remainder. (For an
 *              offset beyond one page the same expression underflowed,
 *              hsize_t being unsigned.)
 *
 *              The read below straddles the first page boundary of a
 *              two-page entry, and the destination is pre-filled with a
 *              sentinel so a short copy is visible rather than benign.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_mpmde_read_across_page(hid_t orig_fapl)
{
    hid_t    fid  = H5I_INVALID_HID; /* File ID */
    H5F_t   *f    = NULL;            /* Internal file object pointer */
    H5PB_t  *pb   = NULL;            /* Page buffer */
    uint8_t *wbuf = NULL;            /* Data written as one multi-page entry */
    haddr_t  addr;                   /* Address of the multi-page entry */
    int64_t  mpmde_before;           /* mpmde_count before the write */
    size_t   i;

    /* The entry spans two pages; the read starts STRADDLE bytes before the
     * first page boundary and continues past it.
     */
    const size_t entry_size = 2 * FS_PAGE_SIZE;
    const size_t straddle   = 8;
    const size_t read_size  = 2 * straddle;

    uint8_t rbuf[16];     /* Must match read_size */
    uint8_t expected[16]; /* Must match read_size */

    H5CX_node_t api_ctx        = {{0}, NULL}; /* API context node to push */
    bool        api_ctx_pushed = false;       /* Whether API context pushed */

    TESTING("H5PB_read() across a page boundary within a multi-page entry");

    if (md_fsm_open_writer(orig_fapl, &fid, &f) != 0)
        FAIL_STACK_ERROR;

    /* H5MF_alloc() and H5F_block_read()/H5F_block_write() are internal calls
     * that expect an API context (H5AC_tag() reads the current tag from it),
     * which only a public API entry point would otherwise establish.
     */
    if (H5CX_push(&api_ctx) < 0)
        FAIL_STACK_ERROR;
    api_ctx_pushed = true;

    pb = f->shared->page_buf;

    if (pb == NULL) {
        printf("Page buffering is not enabled\n");
        TEST_ERROR;
    }
    if (pb->page_size != FS_PAGE_SIZE) {
        printf("Unexpected page size %zu\n", pb->page_size);
        TEST_ERROR;
    }

    if ((wbuf = malloc(entry_size)) == NULL)
        FAIL_STACK_ERROR;
    for (i = 0; i < entry_size; i++)
        wbuf[i] = (uint8_t)(i % 251); /* 251 is prime, so no page-aligned repeat */

    /* Allocate real file space so the entry can be flushed at close */
    if (HADDR_UNDEF == (addr = H5MF_alloc(f, H5FD_MEM_SUPER, (hsize_t)entry_size)))
        FAIL_STACK_ERROR;

    if (addr % FS_PAGE_SIZE != 0) {
        printf("Expected a page-aligned allocation, got %" PRIuHADDR "\n", addr);
        TEST_ERROR;
    }

    mpmde_before = pb->mpmde_count;

    /* A metadata write of at least one page becomes a multi-page entry */
    if (H5F_block_write(f, H5FD_MEM_SUPER, addr, entry_size, wbuf) < 0)
        FAIL_STACK_ERROR;

    /* Without this the rest of the test could pass vacuously: if the write
     * did not actually produce a multi-page entry, the clamp under test is
     * never reached. Compared against a baseline rather than an absolute
     * count, since the library may hold multi-page entries of its own.
     */
    if (pb->mpmde_count != mpmde_before + 1) {
        printf("Expected the write to add 1 multi-page entry (%" PRId64 " -> %" PRId64 ")\n", mpmde_before,
               pb->mpmde_count);
        TEST_ERROR;
    }

    /* Read across the first page boundary of the entry. The sentinel makes a
     * short copy detectable -- with the clamp keyed on page_size, only the
     * first STRADDLE bytes are copied and the rest stay 0xAA.
     */
    memset(rbuf, 0xAA, sizeof(rbuf));

    if (H5F_block_read(f, H5FD_MEM_SUPER, addr + FS_PAGE_SIZE - straddle, read_size, rbuf) < 0)
        FAIL_STACK_ERROR;

    memcpy(expected, wbuf + FS_PAGE_SIZE - straddle, read_size);

    if (memcmp(rbuf, expected, read_size) != 0) {
        printf("Read across the page boundary returned the wrong bytes\n");
        for (i = 0; i < read_size; i++)
            printf("  byte %2zu: got 0x%02X, expected 0x%02X%s\n", i, rbuf[i], expected[i],
                   rbuf[i] == expected[i] ? "" : "   <-- differs");
        TEST_ERROR;
    }

    free(wbuf);
    wbuf = NULL;

    if (H5CX_pop(false) < 0)
        FAIL_STACK_ERROR;
    api_ctx_pushed = false;

    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    PASSED();

    return 0;

error:
    free(wbuf);

    if (api_ctx_pushed)
        H5CX_pop(false);

    H5E_BEGIN_TRY
    {
        H5Fclose(fid);
    }
    H5E_END_TRY;

    return 1;
} /* test_mpmde_read_across_page() */

/*-------------------------------------------------------------------------
 * Function:    check_pb_index_bookkeeping()
 *
 * Purpose:     Verify the page buffer's index accounting against a fresh
 *              walk of the index list.
 *
 *              H5PB.c maintains clean/dirty counts incrementally at each
 *              is_dirty transition. H5PB__DO_SANITY_CHECKS would catch a
 *              missed update, but it is compiled out under NDEBUG -- which
 *              is the build type this branch is normally exercised in --
 *              so this recomputes the totals from the entries themselves.
 *
 *              Recomputation matters: the clean->dirty macro moves bytes
 *              from one counter to the other, so a skipped update leaves
 *              clean + dirty == index_size intact and is invisible to that
 *              sum alone. It shows up only against the real entries.
 *
 * Return:      0 if the accounting is consistent
 *              1 if it is not (details printed)
 *
 *-------------------------------------------------------------------------
 */
static int
check_pb_index_bookkeeping(H5PB_t *pb, const char *when)
{
    const H5PB_entry_t *e;
    int64_t             n_clean = 0, n_dirty = 0;
    int64_t             sz_clean = 0, sz_dirty = 0;
    int64_t             n_all = 0, sz_all = 0;
    int                 ret = 0;

    for (e = pb->il_head; e != NULL; e = e->il_next) {
        n_all++;
        sz_all += (int64_t)e->size;

        if (e->is_dirty) {
            n_dirty++;
            sz_dirty += (int64_t)e->size;
        }
        else {
            n_clean++;
            sz_clean += (int64_t)e->size;
        }
    }

    if (pb->index_len != n_all || pb->index_size != sz_all) {
        printf("%s: index_len/size = %" PRId64 "/%" PRId64 ", walked %" PRId64 "/%" PRId64 "\n", when,
               pb->index_len, pb->index_size, n_all, sz_all);
        ret = 1;
    }
    if (pb->clean_index_size != sz_clean || pb->clean_index_len != n_clean) {
        printf("%s: clean_index_len/size = %" PRId64 "/%" PRId64 ", walked %" PRId64 "/%" PRId64 "\n", when,
               pb->clean_index_len, pb->clean_index_size, n_clean, sz_clean);
        ret = 1;
    }
    if (pb->dirty_index_size != sz_dirty || pb->dirty_index_len != n_dirty) {
        printf("%s: dirty_index_len/size = %" PRId64 "/%" PRId64 ", walked %" PRId64 "/%" PRId64 "\n", when,
               pb->dirty_index_len, pb->dirty_index_size, n_dirty, sz_dirty);
        ret = 1;
    }
    if (pb->index_len != pb->clean_index_len + pb->dirty_index_len) {
        printf("%s: index_len %" PRId64 " != clean %" PRId64 " + dirty %" PRId64 "\n", when, pb->index_len,
               pb->clean_index_len, pb->dirty_index_len);
        ret = 1;
    }
    if (pb->index_size != pb->clean_index_size + pb->dirty_index_size) {
        printf("%s: index_size %" PRId64 " != clean %" PRId64 " + dirty %" PRId64 "\n", when, pb->index_size,
               pb->clean_index_size, pb->dirty_index_size);
        ret = 1;
    }
    if (pb->il_len != pb->index_len || pb->il_size != pb->index_size) {
        printf("%s: index list (%" PRId64 "/%" PRId64 ") disagrees with index (%" PRId64 "/%" PRId64 ")\n",
               when, pb->il_len, pb->il_size, pb->index_len, pb->index_size);
        ret = 1;
    }

    return ret;
} /* check_pb_index_bookkeeping() */

/*-------------------------------------------------------------------------
 * Function:    test_pb_index_bookkeeping()
 *
 * Purpose:     Regression test for the clean/dirty index bookkeeping in
 *              H5PB.c: every is_dirty transition site must call
 *              H5PB__UPDATE_INDEX_FOR_ENTRY_DIRTY() so clean_index_size,
 *              dirty_index_size, and their *_len counterparts stay
 *              consistent with the index's actual entries.
 *
 *              Drives the relevant sites -- a regular page entry going
 *              clean->dirty, a multi-page metadata entry doing the same in
 *              H5PB__write_mpmde(), an mpmde growing, and
 *              H5PB__flush_entry_if_dirty() going dirty->clean -- checking
 *              the accounting after each.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_pb_index_bookkeeping(hid_t orig_fapl)
{
    hid_t    fid = H5I_INVALID_HID; /* File ID */
    H5F_t   *f   = NULL;            /* Internal file object pointer */
    H5PB_t  *pb  = NULL;            /* Page buffer */
    uint8_t *buf = NULL;            /* Data to write */
    haddr_t  small_addr, mpmde_addr;
    int64_t  mpmde_before; /* mpmde_count before an operation */
    size_t   i;

    const size_t small_size = FS_PAGE_SIZE / 4;
    const size_t mpmde_size = 2 * FS_PAGE_SIZE;
    const size_t grown_size = 3 * FS_PAGE_SIZE;

    H5CX_node_t api_ctx        = {{0}, NULL}; /* API context node to push */
    bool        api_ctx_pushed = false;       /* Whether API context pushed */

    TESTING("page buffer clean/dirty index bookkeeping");

    if (md_fsm_open_writer(orig_fapl, &fid, &f) != 0)
        FAIL_STACK_ERROR;

    pb = f->shared->page_buf;

    if (pb == NULL) {
        printf("Page buffering is not enabled\n");
        TEST_ERROR;
    }

    if (H5CX_push(&api_ctx) < 0)
        FAIL_STACK_ERROR;
    api_ctx_pushed = true;

    if ((buf = malloc(grown_size)) == NULL)
        FAIL_STACK_ERROR;
    for (i = 0; i < grown_size; i++)
        buf[i] = (uint8_t)(i % 251);

    if (check_pb_index_bookkeeping(pb, "after open") != 0)
        TEST_ERROR;

    /* A sub-page metadata write: a regular one-page entry, dirty */
    if (HADDR_UNDEF == (small_addr = H5MF_alloc(f, H5FD_MEM_SUPER, (hsize_t)small_size)))
        FAIL_STACK_ERROR;
    if (H5F_block_write(f, H5FD_MEM_SUPER, small_addr, small_size, buf) < 0)
        FAIL_STACK_ERROR;

    if (check_pb_index_bookkeeping(pb, "after sub-page write") != 0)
        TEST_ERROR;

    /* A multi-page metadata write: an mpmde entry, dirty. Allocate room for
     * the later growth up front so both writes stay inside allocated space.
     */
    if (HADDR_UNDEF == (mpmde_addr = H5MF_alloc(f, H5FD_MEM_SUPER, (hsize_t)grown_size)))
        FAIL_STACK_ERROR;

    /* Compare against a baseline rather than an absolute count: flushing the
     * metadata cache can leave multi-page entries of the library's own.
     */
    mpmde_before = pb->mpmde_count;

    if (H5F_block_write(f, H5FD_MEM_SUPER, mpmde_addr, mpmde_size, buf) < 0)
        FAIL_STACK_ERROR;

    if (pb->mpmde_count != mpmde_before + 1) {
        printf("Expected the write to add 1 multi-page entry (%" PRId64 " -> %" PRId64 ")\n", mpmde_before,
               pb->mpmde_count);
        TEST_ERROR;
    }
    if (check_pb_index_bookkeeping(pb, "after multi-page write") != 0)
        TEST_ERROR;

    /* Flush: every dirty entry transitions back to clean */
    if (H5Fflush(fid, H5F_SCOPE_GLOBAL) < 0)
        FAIL_STACK_ERROR;

    if (check_pb_index_bookkeeping(pb, "after flush") != 0)
        TEST_ERROR;

    /* Re-dirty both entries. These are the clean->dirty transitions on
     * entries already in the index -- the case the bug missed.
     */
    if (H5F_block_write(f, H5FD_MEM_SUPER, small_addr, small_size, buf) < 0)
        FAIL_STACK_ERROR;

    if (check_pb_index_bookkeeping(pb, "after re-dirtying the page entry") != 0)
        TEST_ERROR;

    if (H5F_block_write(f, H5FD_MEM_SUPER, mpmde_addr, mpmde_size, buf) < 0)
        FAIL_STACK_ERROR;

    if (check_pb_index_bookkeeping(pb, "after re-dirtying the multi-page entry") != 0)
        TEST_ERROR;

    /* Grow the mpmde: the entry is removed from the index at its old size
     * and re-inserted at the new one, so the accounting has to follow.
     */
    mpmde_before = pb->mpmde_count;

    if (H5F_block_write(f, H5FD_MEM_SUPER, mpmde_addr, grown_size, buf) < 0)
        FAIL_STACK_ERROR;

    if (pb->mpmde_count != mpmde_before) {
        printf("Growing an entry should not change the entry count (%" PRId64 " -> %" PRId64 ")\n",
               mpmde_before, pb->mpmde_count);
        TEST_ERROR;
    }
    if (check_pb_index_bookkeeping(pb, "after growing the multi-page entry") != 0)
        TEST_ERROR;

    /* And once more through a flush, now that sizes have changed */
    if (H5Fflush(fid, H5F_SCOPE_GLOBAL) < 0)
        FAIL_STACK_ERROR;

    if (check_pb_index_bookkeeping(pb, "after second flush") != 0)
        TEST_ERROR;

    free(buf);
    buf = NULL;

    if (H5CX_pop(false) < 0)
        FAIL_STACK_ERROR;
    api_ctx_pushed = false;

    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    PASSED();

    return 0;

error:
    free(buf);

    if (api_ctx_pushed)
        H5CX_pop(false);

    H5E_BEGIN_TRY
    {
        H5Fclose(fid);
    }
    H5E_END_TRY;

    return 1;
} /* test_pb_index_bookkeeping() */

/*-------------------------------------------------------------------------
 * Function:    test_vfd_swmr_refresh_callbacks()
 *
 * Purpose:     Regression test for the pinned-entry eviction gap fixed in
 *              commits b606844abae (v2 B-tree header) and 6361d9dd6b9
 *              (Extensible Array header).
 *
 *              Some metadata cache entries are reference-count-pinned for
 *              the whole open lifetime of the object that owns them -- a
 *              dataset's chunk-index header, for instance, via
 *              H5AC_pin_protected_entry() at rc 0. A VFD SWMR reader's
 *              end-of-tick sweep, H5C_evict_tagged_entries(), can only
 *              unpin flush-dependency pins, never rc-pins, so for such an
 *              entry it must *refresh* the entry in place instead. It
 *              dispatches on the class's refresh callback being non-NULL;
 *              when the slot is NULL the sweep fails outright with
 *              "Pinned entries still need evicted?!".
 *
 *              Both bugs were found only by multi-process scenarios that
 *              happened to apply enough refresh pressure to the right index
 *              type, one type at a time. The underlying invariant is static
 *              and cheap to check, so check it directly: no scenario, no
 *              timing, and it holds in any build type.
 *
 *              NOTE: the list below is deliberately only the classes that
 *              have been fixed. Other rc-pinned classes still have a NULL
 *              refresh slot -- H5AC_FARRAY_HDR most notably, whose pin site
 *              in H5FA__hdr_incr() is identical to the Extensible Array one
 *              that was fixed. Anyone closing one of those gaps should add
 *              the class here.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_vfd_swmr_refresh_callbacks(void)
{
    /* Cache classes that are rc-pinned and therefore must be refreshable */
    const H5AC_class_t *const refreshable[] = {
        H5AC_BT2_HDR,    /* fixed by b606844abae */
        H5AC_EARRAY_HDR, /* fixed by 6361d9dd6b9 */
        H5AC_SUPERBLOCK, /* the precedent both fixes followed */
    };
    size_t   i;
    unsigned nerrs = 0;

    TESTING("VFD SWMR refresh callbacks for pinned cache entries");

    for (i = 0; i < NELMTS(refreshable); i++) {
        if (refreshable[i]->refresh == NULL) {
            printf("Cache class \"%s\" has no VFD SWMR refresh callback; a reader's end-of-tick "
                   "sweep cannot evict its reference-count-pinned entries and will fail with "
                   "\"Pinned entries still need evicted?!\"\n",
                   refreshable[i]->name);
            nerrs++;
        }
    }

    if (nerrs > 0)
        TEST_ERROR;

    PASSED();

    return 0;

error:
    return 1;
} /* test_vfd_swmr_refresh_callbacks() */

/*-------------------------------------------------------------------------
 * Function:    test_deferred_raw_data_free()
 *
 * Purpose:     Verify that a VFD SWMR writer defers raw-data frees instead
 *              of handing the space straight back for reuse.
 *
 *              Readers may be up to max_lag ticks behind and still
 *              referencing space the writer has freed. Raw data is exactly
 *              what the page buffer does not shield them from -- H5PB_read()
 *              and H5PB_write() bypass it for H5FD_MEM_DRAW once
 *              page_buf->vfd_swmr is set -- so deferring the free is the
 *              only thing keeping a lagging reader from reading a new
 *              tenant's bytes as if they were the old data.
 *
 *              This mechanism was silently dropped by the merge that
 *              integrated upstream develop (e2a0f237e50): H5MF_xfree()'s
 *              dispatcher and all three H5MF_process_deferred_frees() call
 *              sites disappeared, leaving H5MF__defer_free() with no callers
 *              at all. Metadata frees are checked too, as the control: those
 *              must still be immediate.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
static unsigned
test_deferred_raw_data_free(hid_t orig_fapl)
{
    hid_t         fid    = H5I_INVALID_HID; /* File ID */
    H5F_t        *f      = NULL;            /* Internal file object pointer */
    H5F_shared_t *shared = NULL;            /* Shared file object pointer */
    haddr_t       meta1, raw1, raw2;        /* Blocks allocated from the HDF5 file */

    const hsize_t blk = 4 * FS_PAGE_SIZE;

    H5CX_node_t api_ctx        = {{0}, NULL}; /* API context node to push */
    bool        api_ctx_pushed = false;       /* Whether API context pushed */

    TESTING("VFD SWMR defers raw-data frees past reader lag");

    if (md_fsm_open_writer(orig_fapl, &fid, &f) != 0)
        FAIL_STACK_ERROR;
    shared = f->shared;

    if (!shared->vfd_swmr_writer) {
        printf("The file is not a VFD SWMR writer\n");
        TEST_ERROR;
    }

    if (H5CX_push(&api_ctx) < 0)
        FAIL_STACK_ERROR;
    api_ctx_pushed = true;

    if (!SIMPLEQ_EMPTY(&shared->lower_defrees)) {
        printf("The deferred-free queue should start out empty\n");
        TEST_ERROR;
    }

    /* Control: metadata frees are not deferred */
    if (HADDR_UNDEF == (meta1 = H5MF_alloc(f, H5FD_MEM_SUPER, blk)))
        FAIL_STACK_ERROR;
    if (H5MF_xfree(f, H5FD_MEM_SUPER, meta1, blk) < 0)
        FAIL_STACK_ERROR;

    if (!SIMPLEQ_EMPTY(&shared->lower_defrees)) {
        printf("A metadata free should not have been deferred\n");
        TEST_ERROR;
    }

    /* Raw-data frees are deferred */
    if (HADDR_UNDEF == (raw1 = H5MF_alloc(f, H5FD_MEM_DRAW, blk)))
        FAIL_STACK_ERROR;
    if (H5MF_xfree(f, H5FD_MEM_DRAW, raw1, blk) < 0)
        FAIL_STACK_ERROR;

    if (SIMPLEQ_EMPTY(&shared->lower_defrees)) {
        printf("A VFD SWMR writer's raw-data free should have been deferred\n");
        TEST_ERROR;
    }

    /* ...and the space is therefore not available again yet */
    if (HADDR_UNDEF == (raw2 = H5MF_alloc(f, H5FD_MEM_DRAW, blk)))
        FAIL_STACK_ERROR;

    if (H5_addr_eq(raw2, raw1)) {
        printf("Freed raw-data block %" PRIuHADDR " was handed straight back; a reader still lagging "
               "behind would read the new tenant's bytes\n",
               raw1);
        TEST_ERROR;
    }

    if (H5CX_pop(false) < 0)
        FAIL_STACK_ERROR;
    api_ctx_pushed = false;

    /* Closing drains the queue outright -- nothing is left to lag behind */
    if (H5Fclose(fid) < 0)
        FAIL_STACK_ERROR;

    PASSED();

    return 0;

error:
    if (api_ctx_pushed)
        H5CX_pop(false);

    H5E_BEGIN_TRY
    {
        H5Fclose(fid);
    }
    H5E_END_TRY;

    return 1;
} /* test_deferred_raw_data_free() */

/*-------------------------------------------------------------------------
 * Function:    main()
 *
 * Purpose:     Main function for VFD SWMR tests.
 *
 * Return:      0 if test is successful
 *              1 if test fails
 *
 *-------------------------------------------------------------------------
 */
int
main(void)
{
    hid_t fapl = H5I_INVALID_HID;      /* File access property list for */
                                       /* data files                    */
    unsigned      nerrors      = 0;    /* Cumulative error count */
    char         *lock_env_var = NULL; /* File locking env var pointer */
    const char   *env_h5_drvr  = NULL; /* File Driver value from environment */
    hbool_t       use_file_locking;    /* Read from env var */
    hid_t         driver_id    = -1;   /* ID for this VFD */
    unsigned long driver_flags = 0;    /* VFD feature flags */

    /* Check the environment variable that determines if we care
     * about file locking. File locking should be used unless explicitly
     * disabled.
     */
    lock_env_var = getenv("HDF5_USE_FILE_LOCKING");
    if (lock_env_var && !strcmp(lock_env_var, "false"))
        use_file_locking = false;
    else
        use_file_locking = true;

    /* Get the VFD to use */
    env_h5_drvr = getenv("HDF5_DRIVER");
    if (env_h5_drvr == NULL)
        env_h5_drvr = "nomatch";

#if 0
    /* Temporary skip testing with multi/split drivers:
     * Page buffering depends on paged aggregation which is
     * currently disabled for multi/split drivers.
     */
    if ((0 == strcmp(env_h5_drvr, "multi")) || (0 == strcmp(env_h5_drvr, "split"))) {
        puts("Skip VFD SWMR test because paged aggregation is disabled for multi/split drivers");
        printf("The %s does not support VFD SWMR feature\n", env_h5_drvr);
        exit(EXIT_SUCCESS);
    }
#endif

#ifdef H5_HAVE_PARALLEL
    puts("Skip VFD SWMR test because paged aggregation is disabled in parallel HDF5");
    exit(EXIT_SUCCESS);
#endif

    /* Set up */
    h5_test_init();

    if ((fapl = h5_fileaccess()) < 0) {
        nerrors++;
        PUTS_ERROR("Can't get VFD-dependent fapl");
    }

    /* Get the VFD feature flags for this VFD */
    if ((driver_id = H5Pget_driver(fapl)) < 0)
        PUTS_ERROR("Can't get driver set in fapl");
    if (H5FDdriver_query(driver_id, &driver_flags) < 0)
        PUTS_ERROR("Can't query driver flags");

    /* Check whether the VFD feature flag supports VFD SWMR */
    if (!(driver_flags & H5FD_FEAT_SUPPORTS_VFD_SWMR)) {
        SKIPPED();
        printf("The %s driver does not support VFD SWMR feature.\n", env_h5_drvr);
        exit(EXIT_SUCCESS);
    }

    if (use_file_locking) {

        nerrors += test_fapl(fapl);
        nerrors += test_file_fapl(fapl);

        nerrors += test_shadow_index_lookup();

#ifndef H5_HAVE_WIN32_API
        /* XXX: VFD SWMR: Fails on Win32 due to problems unlinking the metadata file.
         *                The OS claims another process is using the file.
         */
        nerrors += test_writer_create_open_flush(fapl);
        nerrors += test_writer_md(fapl);
#endif
        nerrors += test_reader_md_concur(fapl);

        nerrors += test_multiple_file_opens(fapl);
        nerrors += test_multiple_file_opens_concur(fapl);

        nerrors += test_enable_disable_eot(fapl);
        nerrors += test_enable_disable_eot_concur(fapl);

        nerrors += test_file_end_tick(fapl);
        nerrors += test_file_end_tick_concur(fapl);

        nerrors += test_updater_flags(fapl);
        nerrors += test_updater_flags_same_file_opens(fapl);

#ifndef H5_HAVE_WIN32_API
        /* VFD SWMR: Fails on windows due to error from generate_md_ck_cb(). */
        nerrors += test_updater_generate_md_checksums(fapl, true);
        nerrors += test_updater_generate_md_checksums(fapl, false);
#endif
        nerrors += test_same_file_opens(fapl, false);
        nerrors += test_same_file_opens(fapl, true);
        nerrors += test_vfds_same_file_opens(fapl, env_h5_drvr);

        nerrors += test_make_believe_multiple_file_opens_concur(fapl);

        nerrors += test_auto_generate_md(fapl, NULL);
        nerrors += test_auto_generate_md(fapl, ".");
        nerrors += test_auto_generate_md(fapl, "./");
        nerrors += test_auto_long_md_path_name(fapl);
        nerrors += test_long_md_path_name(fapl);

#ifndef H5_HAVE_WIN32_API
        /* XXX: VFD SWMR: Fails on Win32 due to problems unlinking the metadata file.
         *                The OS claims another process is using the file.
         */
        nerrors += test_md_alloc_free(fapl);
        nerrors += test_md_try_extend(fapl);
        nerrors += test_md_try_shrink(fapl);
        nerrors += test_md_sect_alignment(fapl);
        nerrors += test_mpmde_read_across_page(fapl);
        nerrors += test_pb_index_bookkeeping(fapl);
        nerrors += test_vfd_swmr_refresh_callbacks();
        nerrors += test_deferred_raw_data_free(fapl);
#endif
    }

    h5_cleanup(namebases, fapl);

    if (nerrors)
        goto error;

    puts("All VFD SWMR tests passed.");

    exit(EXIT_SUCCESS);

error:
    printf("***** %d VFD SWMR TEST%s FAILED! *****\n", nerrors, nerrors > 1 ? "S" : "");

    H5E_BEGIN_TRY
    {
        H5Pclose(fapl);
    }
    H5E_END_TRY;
}
