//
// Created by root on 18-9-13.
//

#include <iostream>
#include <sysqos_common.h>
#include <awe_conf/env.h>
#include "client.h"
#include "server.h"
#include "demo.h"

void client::submit_tasks(task_group *task_grp)
{
    unique_lock<std::mutex> lck(m);
    tasks_list.push_back(task_grp);
//    if ( task_grp->cli->name() == string("c5") )
//    {
//        cout << RED << "client::submit_tasks( " << RESET"(" << task_grp->task_id
//                << ")" << name()
//                << ")task_retry:" << task_retry
//                << endl;
//    }
}

void client::submit_sure_tasks(task_group *task_grp)
{
    unique_lock<std::mutex> lck(m);
    sure_tasks_list.push_back(task_grp);
//    if ( task_grp->cli->name() == string("c5") )
//    {
//        cout << BLUE << "client::submit_sure_tasks( " << RESET"("
//                << task_grp->task_id
//                << ")" << name()
//                << ")task_retry:" << task_retry
//                << endl;
//    }
}

bool client::could_snd(task_group *tgrp, bool &is_back_toq)
{

    is_back_toq = false;
    if ( is_qos_open )
    {
        resource  rs[50];
        for ( int i   = 0; i < tgrp->tasks.size(); ++i )
        {
            rs[i].cost = tgrp->tasks[i]->cost;
            rs[i].id   = (void *) tgrp->tasks[i]->srv->name().c_str();
        }
        int       err = ops.get_token_grp(&ops, (int) tgrp->tasks.size(), rs,
                                          tgrp,
                                          (void **) &tgrp->token);
        if ( QOS_ERROR_OK == err )
        {
            return true;
        }
        if ( QOS_ERROR_PENDING == err )
        { return false; }
        else
        {
            is_back_toq = true;
            return false;
        }
    }
    return true;
}

env demo_retry_interval("demo", "demo_retry_interval");

bool client::run_once()
{
    unique_lock<std::mutex> lck(m);
    if ( tasks_list.empty() && complete_list.empty() &&
         sure_tasks_list.empty() )
    {
        return false;
    }
    
    if ( !sure_tasks_list.empty() )
    {
        task_group *tgrp = sure_tasks_list.front();
        sure_tasks_list.pop_front();
//        if ( name() == string("c5") )
//        {
//            cout << YELLOW << "client::run_once " << RESET"(" << tgrp->task_id
//                    << ")"
//                    << name()
//                    << ")task_retry:" << task_retry
//                    << " !sure_tasks_list.empty() before snd_tasks"
//                    << endl;
//        }
        snd_tasks(tgrp);
        return true;
    }
    
    if ( !complete_list.empty() )
    {
        task *t = complete_list.front();
        complete_list.pop_front();
//        if ( name() == string("c5") )
//        {
//            cout << YELLOW << "client::run_once " << RESET"(" << t->task_id
//                    << ")"
//                    << name() << "->" << t->srv->name() << " retry_num("
//                    << t->retry_num
//                    << ")task_retry:" << task_retry
//                    << " before  if ( t->retry_num > 0 && t->retry_num < task_retry )"
//                    << endl;
//        }
        
        if ( t->retry_num > 0 && t->retry_num < task_retry )
        {
            lck.unlock();
            if ( demo_retry_interval.get_int() > 0 )
            {
                usleep(demo_retry_interval.get_int());
            }
            retry_task(t);
            return true;
        }
        else if( t->retry_num > 0)
        {
            t->set_stat(task_stat_complete);
            lck.unlock();
            t->grp->set_failed();
            lck.lock();
        }
//        if ( name() == string("c5") )
//        {
//            cout << RED << "client::run_once " << RESET"(" << t->task_id << ")"
//                    << name() << "->" << t->srv->name() << " retry_num("
//                    << t->retry_num
//                    << ")task_retry:" << task_retry
//                    << RED<<" final_cnt:"<<RESET << t->grp->final_cnt
//                    << " t->wait_grp_cnt:" << t->grp->wait_grp_cnt
//                    << " before final_task"
//                    << endl;
//        }
        if ( final_task(t) )
        {
            lck.unlock();
            if ( is_qos_open )
            {
                resource_t rs[1];
//                if ( name() == string("c5") )
//                {
//                    cout << YELLOW "client::run_once (" << t->grp->task_id
//                            << ")"
//                            << RESET
//                            << t->grp->cli->name()
//                            << "| before put_token_grp:" << endl;
//                }
                ops.put_token_grp(&ops, t->grp->token, 0, rs);
//                if ( name() == string("c5") )
//                {
//                    cout << GREEN << "client::run_once (" << t->grp->task_id
//                            << ")"
//                            << RESET
//                            << t->grp->cli->name()
//                            << "| after put_token_grp:" << endl;
//                }
            
            }
            
            hook->task_final(t->grp);
            lck.lock();
        }
    }
    if ( !tasks_list.empty() )
    {
        bool       is_back_toq;
        task_group *tgrp = tasks_list.front();
        tasks_list.pop_front();
        if ( could_snd(tgrp, is_back_toq) )
        {
            lck.unlock();
            snd_tasks(tgrp);
        }
        else if ( is_back_toq )
        {
            tasks_list.push_front(tgrp);
            lck.unlock();
            usleep(1);
            return false;
        }
    }
    return true;
}


string client::name() const
{
    return cli_name;
}

static void token_grp_got(void *token_grp, void *pri, int err)
{
    
    assert(err == 0);
    task_group *tg = (task_group *) pri;
    
    tg->token = (token_reqgrp *) token_grp;
//    if ( tg->cli->name() == string("c5") )
//    {
//        cout << RED << "token_grp_got (" << tg->task_id << ")" << RESET
//                << tg->cli->name()
//                << "| before submit_sure_tasks:" << endl;
//    }
    tg->cli->submit_sure_tasks(tg);
//    if ( tg->cli->name() == string("c5") )
//    {
//        cout << BLUE << "token_grp_got (" << tg->task_id << ")" << RESET
//                << tg->cli->name()
//                << "| before submit_sure_tasks:" << endl;
//    }
}

static sysqos_applicant_event_ops_t event_ops =
                                            {
                                                    .token_grp_got = token_grp_got
                                            };

client::client(const string &_name, bool qos_open, int _grp_retry,
               int _task_retry, complete_task_hook *_hook) :
        cli_name(_name), is_qos_open(qos_open), hook(_hook),
        grp_retry(_grp_retry), task_retry(_task_retry)
{
    assert(0 == sysqos_app_init(&ops, &event_ops, demo_compare, demo_hash));
}

void client::snd_tasks(task_group *tgrp)
{
    tgrp->set_stat(task_group_stat_wait_grp);
    for ( auto t : tgrp->tasks )
    {
        t->set_stat(task_stat_net_send);
        t->srv->submit_task(t);
    }
}

static env is_fix_quota("demo", "is_fix_quota");

void client::retry_task(task *t)
{
    unique_lock<std::mutex> lck(m);
//    if ( t->cli->name() == string("c5") )
//    {
//        cout << RED << "client::retry_task " << RESET"(" << t->task_id << ")"
//                << name() << "->" << t->srv->name() << " retry_num("
//                << t->retry_num
//                << ")task_retry:(" << task_retry
//                << endl;
//    }
    t->set_stat(task_stat_net_send);
    lck.unlock();
    t->srv->submit_task(t);
}

void client::complete_task(task *t)
{
    
    if ( is_qos_open && !is_fix_quota.get_int() )
    {
        long len = t->srv->ops
                .snd_msg_len(&t->srv->ops, (void *) cli_name.c_str());
        assert(len <= 1024);
        unsigned char buf[1024];
        t->srv->ops
                .snd_msg_buf(&t->srv->ops, (void *) cli_name.c_str(), len, buf);
//        if ( name() == string("c5") )
//        {
//            cout << YELLOW << "client::complete_task (" << t->task_id << ")"
//                    << RESET
//                    << name() << " -> "
//                    << t->srv->name() << ":"
//                    << t->string_bytaskstat(t->state)
//                    << "| retry_num " << t->retry_num
//                    << "| before rcvd:"
//                    << complete_list.size() << endl;
//        }
        ops.rcvd(&ops, (void *) t->srv->name().c_str(), len, buf);
//        if ( name() == string("c5") )
//        {
//            cout << RED << "client::complete_task (" << t->task_id << ")"
//                    << RESET
//                    << name() << " -> "
//                    << t->srv->name() << ":"
//                    << t->string_bytaskstat(t->state)
//                    << "| retry_num " << t->retry_num
//                    << "| after rcvd:"
//                    << complete_list.size() << endl;
//        }
    }
    unique_lock<std::mutex> lck(m);
//    if ( name() == string("c5") )
//    {
//        cout << YELLOW << "client::complete_task (" << t->task_id << ")"
//                << RESET
//                << name() << " -> "
//                << t->srv->name() << ":"
//                << t->string_bytaskstat(t->state)
//                << "| retry_num " << t->retry_num
//                << "| complete_list:"
//                << complete_list.size() << endl;
//    }
    complete_list.push_back(t);
    lck.unlock();
//    hook->task_completed();
}

bool client::final_task(task *t)
{
    return t->grp->is_final();
}
