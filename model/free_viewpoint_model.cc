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
#include "free_viewpoint_model.h"
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Free_Viewpoint_Model");

NS_OBJECT_ENSURE_REGISTERED (Free_Viewpoint_Model);

Free_Viewpoint_Model::Free_Viewpoint_Model ()
{
  /*    m_nViews = 9;
    m_minDwellTime = 2;

    m_cumulativeProb = {0.6, 0.7, 0.8, 0.85, 0.9, 0.925, 0.95, 0.975, 1.0};
    m_avgDwellTime = {10,5,5,5,5,8,8,5,5};*/

  m_nViews = 5;
  m_minDwellTime = 300;

  m_cumulativeProb = {0.4, 0.6, 0.80, 0.90, 1.0};
  m_avgDwellTime = {4,2,2,3,3};



  ns3::RngSeedManager::SetSeed (2);

  m_pUniRNG = CreateObject<UniformRandomVariable> ();
  m_pUniRNG->SetAttribute ("Min", DoubleValue (0.0));
  m_pUniRNG->SetAttribute ("Max", DoubleValue (1.0));

  m_pExpRNG = CreateObject<ExponentialRandomVariable> ();
  m_pExpRNG->SetAttribute ("Bound", DoubleValue (30.0));
}

Free_Viewpoint_Model::Free_Viewpoint_Model (const std::string free_viewpoint_file)
{
  Free_Viewpoint_Model ();
}

int32_t
Free_Viewpoint_Model::InitViewpoint ()
{
  int32_t viewpoint = 0;

  //    m_remDwellTime = m_minDwellTime + ceil(m_pExpRNG->GetValue(m_avgDwellTime.at(viewpoint), 30));
  m_remDwellTime = 2 * m_minDwellTime;
  //    NS_LOG_INFO("InitViewpoint - Dwell Time : " << m_remDwellTime);
  return viewpoint;
}

int32_t
Free_Viewpoint_Model::GetNextViewpoint (const int64_t t_index)
{
  int32_t viewpoint = CurrentViewpoint ();
  m_remDwellTime -= 1;
  if (m_remDwellTime <= 0)
    { // Switch Viewpoint
      double ranval = m_pUniRNG->GetValue ();
      viewpoint = 0;
      for (double prob : m_cumulativeProb)
        {
          if (ranval <= prob)
            break;
          viewpoint += 1;
        }
      
      m_remDwellTime =
          m_minDwellTime + ceil (m_pExpRNG->GetValue (m_avgDwellTime.at (viewpoint), 10));

    }
  return viewpoint;
}
} // namespace ns3