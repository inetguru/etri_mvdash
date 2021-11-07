/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2021 ETRI (Electronic and Telecommunication Research Institute) KOREA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/free_viewpoint_model.h"
#include "ns3/markovian_viewpoint_model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("modeltest");

int main(int argc, char *argv[]) {
    LogLevel log_precision = LOG_LEVEL_INFO; // LOG_LEVEL_INFO; LOG_LEVEL_LOGIC; LOG_ALL;
    LogComponentEnable ("Free_Viewpoint_Model", log_precision);
    LogComponentEnable ("Markovian_Viewpoint_Model", log_precision);
    LogComponentEnable("modeltest", log_precision);

    std::string path = "./contrib/etri_mvdash/";
    std::string vpInfo = "viewpoint_transition.csv";

    CommandLine cmd;
    cmd.AddValue ("vpInfo", "The name of the file containing viewpoint switching info",vpInfo);
    cmd.Parse (argc, argv);
        
    int old_viewpoint, new_viewpoint = -1;
    int64_t t_index = 0;

    Markovian_Viewpoint_Model * pView = new Markovian_Viewpoint_Model(path+vpInfo);
//    Free_Viewpoint_Model * pView = new Free_Viewpoint_Model();

    for ( ; t_index < 100; t_index++) {
        old_viewpoint = new_viewpoint;
        new_viewpoint = pView->UpdateViewpoint(t_index);
        if (old_viewpoint != new_viewpoint)
            NS_LOG_INFO("Viewpoint change at T[" << t_index << "] " << new_viewpoint);
    } 
}