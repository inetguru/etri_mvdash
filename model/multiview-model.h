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
#ifndef MULTIVIEW_MODEL_H
#define MULTIVIEW_MODEL_H

#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"

namespace ns3 {

struct st_viewpointData 
{
  std::vector <int32_t> viewpointIndex; //!< Index of the view point
  std::vector <int64_t> viewpointTime;  //!< Point in time in microseconds when this viewpoint is selected
};

class MultiView_Model : public Object
{
public:
  MultiView_Model () {};
  int32_t UpdateViewpoint(const int64_t t_index);
  int32_t CurrentViewpoint() { return m_viewpointData.viewpointIndex.back();}
  int32_t PrevViewpoint() { return m_viewpointData.viewpointIndex[ m_viewpointData.viewpointIndex.size()-2];} //DION_TEST
  std::vector <int32_t> getVPindex() {return m_viewpointData.viewpointIndex;}
  double ViewpointRatio(int viewpoint) 
    { return (double)m_nViewpointSelected.at(viewpoint)/m_viewpointData.viewpointIndex.size();}
  int32_t m_nViews;

protected:
  virtual int32_t InitViewpoint()=0;
  virtual int32_t GetNextViewpoint(const int64_t t_index)=0;
  st_viewpointData m_viewpointData;
  std::vector <int32_t> m_nViewpointSelected;
};
} // namespace ns3

#endif /* MULTIVIEW_MODEL_H */