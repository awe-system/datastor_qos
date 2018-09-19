//
// Created by root on 18-9-13.
//

#include "demo_obj.h"
#include "client.h"
#include "task.h"

void demo_obj::run()
{
    while ( !is_stop )
    {
        usleep(1);
        unique_lock<std::mutex> lck(run_lck);
//        cond.wait(lck);
        while ( !to_del_list.empty() || !to_snd_task_list.empty() )
        {
            task_group *t_grp = NULL;
            if ( !to_del_list.empty() )
            {
                t_grp = to_del_list.front();
                to_del_list.pop_front();
                wait_task_map.erase(t_grp->task_id);
                lck.unlock();
                try_free_grp(t_grp);
            }
            else
            {
                lck.unlock();
            }
            
            lck.lock();
            if ( !to_snd_task_list.empty() )
            {
                t_grp = to_snd_task_list.front();
                to_snd_task_list.pop_front();
                wait_task_map.insert(make_pair(t_grp->task_id, t_grp));
                lck.unlock();
                try_increase_grp(t_grp);
            }
            else
            {
                lck.unlock();
            }
            lck.lock();
        }
    }
}

void demo_obj::task_final(task_group *tgrp)
{
    unique_lock<std::mutex> lck(run_lck);
    to_del_list.push_back(tgrp);
    cond.notify_one();
}

void demo_obj::addtask(const int task_num, const int cost,
                        const json_obj &c_s_pairs)
{
   
    map<server *, client *> cs_map;
 
    for ( int i = 0; i < task_num; ++i )
    {
       
        for ( auto              jpair : c_s_pairs.dic_val )
        {
            client *cli = clients->client_byname(jpair.second.s_val);
            server *srv = servers->server_byname(jpair.first.s_val);
            cs_map.insert(make_pair(srv, cli));
        }
        task_group              *tgrp;
        tgrp = new task_group(cost, cs_map,
                              __sync_fetch_and_add(&cur_task_id, 1));
        
        for ( auto              t :tgrp->tasks )
        {
            auto it = cli_sers.find(t->cli->name() + t->srv->name());
            it->second->add_task(t);
        }
        to_snd_task_list.push_back(tgrp);
    }
}

json_obj demo_obj::to_json_obj()
{
    json_obj obj;
    obj.set_dic();
//    obj.merge(json_obj("clients", clients->to_json_obj()));
//    obj.merge(json_obj("servers", servers->to_json_obj()));
//    obj.merge(json_obj("tasks", task_detail()));
    obj.merge(json_obj("csinfo", json_by_cs()));
    return obj;
}

json_obj demo_obj::task_detail()
{
    json_obj tasks;
    tasks.set_array();
    std::unique_lock<std::mutex> mlck(run_lck);
    for ( auto                   t : to_snd_task_list )
    {
        tasks.append(t->to_json_obj());
    }
    for ( auto                   p : wait_task_map )
    {
        tasks.append(p.second->to_json_obj());
    }
    return tasks;
}

demo_obj::demo_obj(client_group *clis, server_group *servs) :
        cur_task_id(0), clients(clis), servers(servs), is_stop(false)
{
    for ( auto cli_it :clis->clients )
    {
        for ( auto srv_it : servs->servers )
        {
            client  *cli = cli_it.second;
            server  *srv = srv_it.second;
            cli_ser *cs  = new cli_ser(cli, srv);
            cli_sers.insert(make_pair(cli->name() + srv->name(), cs));
        }
    }
    clients->set_hook(this);
}

json_obj demo_obj::json_by_cs()
{
    json_obj obj;
    obj.set_array();
    for ( auto it : cli_sers )
    {
        obj.append(it.second->to_json_obj());
    }
    return obj;
}

void demo_obj::update_demo_analysis(task_group *grp)
{
    for (auto t : grp->tasks)
    {
        client *cli = t->cli;
        server *srv = t->srv;
        cli_sers[cli->name()+srv->name()]->update(t);
    }
}

void demo_obj::try_free_grp(task_group *grp)
{
    update_demo_analysis(grp);
    for ( auto t :grp->tasks )
    {
        auto it = cli_sers.find(t->cli->name() + t->srv->name());
        it->second->rm_task(t);
    }
    delete grp;
}

void demo_obj::try_increase_grp(task_group *grp)
{
    grp->set_stat(task_group_stat_wait_token);
    clients->submit_tasks(grp);
}

void demo_obj::addtasks(const json_obj &obj)
{
    try
    {
        unique_lock<std::mutex> lck(run_lck);
        long num = obj["num"].get_number();
        json_obj obj_array = obj["taskinfo"];
        for(long i = 0;i<num;++i)
        {
            for(const json_obj &task_obj : obj_array.array_val)
            {
                addtask((const int) task_obj["task_num"].get_number(),
                         (const int) task_obj["task_cost"].get_number(),
                         task_obj["c_s_pairs"]);
            }
        }
        
        return;
    }
    catch (...)
    {
        return;
    }
}
