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

 #include "multiview-model.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MultiView_Model");

NS_OBJECT_ENSURE_REGISTERED (MultiView_Model);

int32_t MultiView_Model::UpdateViewpoint(const int64_t t_index)
{
    int32_t viewpoint;
    
    if (t_index == 0) { 
        viewpoint = InitViewpoint();
        m_nViewpointSelected.assign(m_nViews, 0);
    }
    else 
        viewpoint = GetNextViewpoint(t_index);

    m_viewpointData.viewpointIndex.push_back(viewpoint);
    m_viewpointData.viewpointTime.push_back(Simulator::Now ().GetMicroSeconds ());
    m_nViewpointSelected.at(viewpoint) += 1;
        
    return viewpoint;
}

} // namespace ns3