cmake_minimum_required(VERSION 3.7)
project(test_qos)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        list_head.h
        sysqos_interface.h
        sysqos_token_reqgrp_manager.c
        sysqos_token_reqgrp_manager.h
        hash_table.c hash_table.h
        memory_cache.c memory_cache.h
        sysqos_alloc.c sysqos_alloc.h sysqos_common.h
        safe_rw_list.c safe_rw_list.h
        sysqos_container_item.h  sysqos_nodereq_list.c sysqos_nodereq_list.h
        sysqos_policy_param.h sysqos_dispatch_base_node.c sysqos_dispatch_base_node.h
        sysqos_protocol.h sysqos_token_reqgrp.c sysqos_token_reqgrp.h
        sysqos_dispatch_node.c sysqos_dispatch_node.h
        sysqos_dispatch_manager.c sysqos_dispatch_manager.h sysqos_token_global.c
        sysqos_token_global.h sysqos_app_node.c sysqos_app_node.h
        sysqos_token_update.c sysqos_token_update.h sysqos_update_node.c
        sysqos_update_node.h
        sysqos_waitincrease_list.c sysqos_waitincrease_list.h count_controller.c count_controller.h fence_executor.c fence_executor.h)

include_directories("test_lib")
include_directories(".")


add_library(libtest test_lib/sysqos_test_lib.c test_lib/sysqos_test_lib.h test_lib/test_frame.c test_lib/test_frame.h)
target_link_libraries(libtest pthread)

add_library(libqos ${SOURCE_FILES} )
target_link_libraries(libqos pthread libtest)

add_compile_options("-fprofile-arcs")
add_compile_options("-ftest-coverage")

add_executable(test_app2disp test_app2dispatch/app2disp_test.c test_app2dispatch/test_memory_cache.c test_app2dispatch/test_memory_cache.h test_app2dispatch/test_rw_list.c test_app2dispatch/test_rw_list.h)
target_link_libraries(test_app2disp pthread cunit libqos libtest gcov)


add_executable(test_app test_applicant/app_test.c)
target_link_libraries(test_app pthread cunit libqos libtest gcov)