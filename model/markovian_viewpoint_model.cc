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
#include "ns3/log.h"
#include <ns3/core-module.h>
#include "markovian_viewpoint_model.h"
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Markovian_Viewpoint_Model");

NS_OBJECT_ENSURE_REGISTERED (Markovian_Viewpoint_Model);

Markovian_Viewpoint_Model::Markovian_Viewpoint_Model()
{
    m_nViews = 5;
    m_minDwellTime = 2;
    m_UpperBound_ExpRNG = 10.0;

    for (int vp = 0; vp < m_nViews; vp++) {
        m_cumulativeTransitionMatrix.push_back({0.4, 0.6, 0.8, 0.9, 1.0});
        m_avgDwellTimeMatrix.push_back({4,2,2,3,3});
    }

    ns3::RngSeedManager::SetSeed(2);

    m_pUniRNG = CreateObject<UniformRandomVariable>();
    m_pUniRNG->SetAttribute ("Min", DoubleValue(0.0));
    m_pUniRNG->SetAttribute ("Max", DoubleValue(1.0));

    m_pExpRNG = CreateObject<ExponentialRandomVariable>();
}

Markovian_Viewpoint_Model::Markovian_Viewpoint_Model(const std::string viewpoint_file)
{
    NS_LOG_FUNCTION (this);
    std::ifstream vpfile;
    vpfile.open (viewpoint_file.c_str ());
    if (!vpfile) {
        NS_LOG_INFO("File Open Error");
        return;
    }
    std::string temp;
    std::getline(vpfile, temp);     // Get the first line
    std::istringstream buffer(temp);
    std::vector<int32_t> first_line ((std::istream_iterator<int32_t> (buffer)),
                 std::istream_iterator<int32_t>());

    // Parse the first line
    int switching_model_type = first_line[0];
    m_nViews = first_line[1];
    m_minDwellTime = first_line[2];
    m_UpperBound_ExpRNG = (double) first_line[3];
    ns3::RngSeedManager::SetSeed(first_line[4]);    // first_line[4] --> Random Seed Values
    m_pUniRNG = CreateObject<UniformRandomVariable>();
    m_pUniRNG->SetAttribute ("Min", DoubleValue(0.0));
    m_pUniRNG->SetAttribute ("Max", DoubleValue(1.0));
    m_pExpRNG = CreateObject<ExponentialRandomVariable>();    

    if (switching_model_type == SimpleType) {
        int vp_i, vp_j;
        for (vp_i=0; vp_i < m_nViews; vp_i++) {
            std::getline (vpfile, temp);
            std::istringstream buffer(temp);
            std::vector<double> prob_trans ((std::istream_iterator<double> (buffer)),
                 std::istream_iterator<double>());
            
            for (vp_j=1; vp_j < m_nViews; vp_j++) {
                prob_trans[vp_j] += prob_trans[vp_j-1];
//              NS_LOG_INFO("[" << vp_i << "," << vp_j << "] = " << prob_trans[vp_j]);
            }
            m_cumulativeTransitionMatrix.push_back(prob_trans);           
        }
        for (vp_i=0; vp_i < m_nViews; vp_i++) {
            std::getline (vpfile, temp);
            std::istringstream buffer(temp);
            std::vector<double> dwell_time ((std::istream_iterator<double> (buffer)),
                 std::istream_iterator<double>());            
/*          for (vp_j=1; vp_j < m_nViews; vp_j++) {
                NS_LOG_INFO("[" << vp_i << "," << vp_j << "] = " << dwell_time[vp_j]);
            }*/
            m_avgDwellTimeMatrix.push_back(dwell_time);           
        }
    } 

       
    vpfile.close();
}

int32_t Markovian_Viewpoint_Model::InitViewpoint() {
    int32_t  viewpoint = 0;
    m_remDwellTime = 2*m_minDwellTime;
    return viewpoint;
}

int32_t Markovian_Viewpoint_Model::GetNextViewpoint(const int64_t t_index) {
    int32_t viewpoint = CurrentViewpoint();
    m_remDwellTime -= 1;
    if (m_remDwellTime <= 0) {  // Switch Viewpoint
        double ranval = m_pUniRNG->GetValue();
        int old_viewpoint = viewpoint;
        viewpoint = 0;
        for (double prob : m_cumulativeTransitionMatrix[old_viewpoint]) {
            if (ranval <= prob)
                break;
            viewpoint += 1;
        }
        m_remDwellTime = m_minDwellTime + ceil(m_pExpRNG->GetValue(m_avgDwellTimeMatrix[old_viewpoint][viewpoint], m_UpperBound_ExpRNG));
    }
    return viewpoint; 
}
} // namespace ns3