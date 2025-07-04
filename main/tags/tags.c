#include "tags.h"
#include "bsp.h"
#include "client.h"
#include "nvstate.h"
#include "log.h"

#include "cJSON.h"

#include <stdio.h>

// Filename where the authorized tags are saved
#define TAGS_FILENAME "tags.json"

status_t tag_sync_handler(msg_t *msg);

// Open file with tags
file_t tag_file = NULL;

status_t tags_init(void)
{
    tag_file = fs_open(TAGS_FILENAME, "r");
    if (tag_file == NULL)
    {
        ERROR("Couldn't open %s", TAGS_FILENAME);
        return -STATUS_NOFILE;
    }

    // When new firmware is written the tags file is lost, but the nvstate 
    // partition is usually untouched (unless the flash is explicitly erased). 
    // In this case, a sync message with the same tags list will be rejected 
    // until a change is made to the list (and thus changing the hash).
    //
    // Here we check if the tag file is empty. If so, the hash is cleared so 
    // the sync message can populate the tags list here.
    char card_str[16];
    if (fs_readline(tag_file, card_str) == -STATUS_EOF)
    {
        uint8_t tag_hash[TAG_HASH_LEN];
        memset(tag_hash, 0, TAG_HASH_LEN);
        nvstate_tag_hash_set(tag_hash, TAG_HASH_LEN);
        fs_rewind(tag_file);
    }

    // Register a handler with the client to handle incoming sync messages
    return client_handler_register(tag_sync_handler);
}

status_t tags_verify(uint32_t card)
{
    // Go line-by-line through the file, reading card data and comparing 
    // against card. If EOF is reached before a match is found, no match 
    // exists.
    char card_str[16];
    status_t status = fs_readline(tag_file, card_str);
    while (status != -STATUS_EOF) 
    {
        uint32_t db_card = atoi(card_str);
        if (db_card == card)
        {
            // Be kind
            fs_rewind(tag_file);
            return STATUS_OK;
        }
        status = fs_readline(tag_file, card_str);
    }

    fs_rewind(tag_file);
    return -STATUS_INVALID;
}

status_t tag_sync_handler(msg_t *msg)
{
    // Only handle sync messages
    if (msg->type == MSG_SYNC)
    {
        // Get the existing hash
        size_t hash_len;
        uint8_t cur_hash[TAG_HASH_LEN];
        nvstate_tag_hash(cur_hash, &hash_len);

        INFO("New authorized card list received");

        // Only update the tags if the hashes are different
        if (memcmp(cur_hash, msg->sync.hash, TAG_HASH_LEN) != 0)
        {
            WARN("saving...");
            char file_line[16];

            // Close and delete the old file. We'll create a new file and 
            // rewrite it.
            fs_close(tag_file);
            fs_rm(TAGS_FILENAME);
            file_t new_file = fs_open(TAGS_FILENAME, "w");
            if (new_file == 0)
            {
                ERROR("Couldn't save new cards");
                return STATUS_OK;
            }
    
            // Parse the Received JSON. The tags are provided in an array, and 
            // we can iterate over it.
            cJSON *tag;
            cJSON_ArrayForEach(tag, msg->sync.tags)
            {
                // We're reformatting: each card is delimited with a line feed.
                int len = sprintf(file_line, "%d\n", atoi(tag->valuestring));
                fs_write(new_file, file_line, len);
            }

            // Reopen the file as read-only
            fs_close(new_file);
            tag_file = fs_open(TAGS_FILENAME, "r");

            // Store the curernt hash
            nvstate_tag_hash_set(msg->sync.hash, TAG_HASH_LEN);

            WARN("Done saving cards");
        }
        else
        {
            INFO("Hash matches stored, skip write");
        }
        return STATUS_OK;
    }
    return -STATUS_UNAVAILABLE;
}