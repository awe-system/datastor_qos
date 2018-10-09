
#include "test_frame.h"
#include "test_applicant/node_inapp.h"
#include "test_applicant/wait_token_list.h"
#include "test_applicant/token_grp.h"
#include "test_applicant/token_manager.h"
#include "test_dispatch/node_indisp.h"
#include "test_dispatch/wait_increase.h"
#include "test_dispatch/token_global.h"
#include "test_dispatch/dispatch_manager.h"
#include "test_app2dispatch/test_memory_cache.h"

#include "test_app2dispatch/test_rw_list.h"
#include "test_app2dispatch/test_hashtab.h"

int main(int argc, char *argv[])
{
    test_frame_t frame;

    test_frame_init(&frame);
    memory_cache_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    safe_rw_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    hashtab_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    node_inapp_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    wait_list_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    token_reqgrp_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    token_manager_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    base_node_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    test_frame_init(&frame);
    
    node_indisp_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    wait_increase_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    token_global_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);
    
    test_frame_init(&frame);
    dispatch_manager_suit_init(&frame);
    frame.run(&frame);
    test_frame_exit(&frame);

    return 0;
}