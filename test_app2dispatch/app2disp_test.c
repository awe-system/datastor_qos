

#include <test_frame.h>
#include "test_memory_cache.h"

#include "safe_rw_list.h"
#include "hash_table.h"
#include "sysqos_dispatch_base_node.h"
#include "sysqos_nodereq_list.h"
#include "sysqos_token_reqgrp.h"
#include "sysqos_dispatch_node.h"
#include "sysqos_token_reqgrp_manager.h"
#include "test_rw_list.h"
#include "test_hashtab.h"


int main(int argc, char *argv)
{
    test_frame_t frame;
    
    test_frame_init(&frame);
    
    memory_cache_suit_init(&frame);
    
    safe_rw_suit_init(&frame);
    
    hashtab_suit_init(&frame);
    
    frame.run(&frame);
    
    test_frame_exit(&frame);
//
//    test_hash_table();
//    test_dispatch_base_node();
//    test_wait_token_list();
//    test_token_reqgrp();
//    test_dispatch_node();
//    test_token_reqgrp_manager();
//    test_wait_increase_list();
    return 0;
}
