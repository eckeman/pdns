#include "dnsdist.hh"
#include "lock.hh"

size_t Rings::numDistinctRequestors()
{
  std::set<ComboAddress, ComboAddress::addressOnlyLessThan> s;
  ReadLock rl(&queryLock);
  for(const auto& q : queryRing)
    s.insert(q.requestor);
  return s.size();
}

std::unordered_map<int, vector<boost::variant<string,double>>> Rings::getTopBandwidth(unsigned int numentries)
{
  map<ComboAddress, unsigned int, ComboAddress::addressOnlyLessThan> counts;
  uint64_t total=0;
  {
    ReadLock rl(&queryLock);
    for(const auto& q : queryRing) {
      counts[q.requestor]+=q.size;
      total+=q.size;
    }
  }

  {
    std::lock_guard<std::mutex> lock(respMutex);
    for(const auto& r : respRing) {
      counts[r.requestor]+=r.size;
      total+=r.size;
    }
  }

  typedef vector<pair<unsigned int, ComboAddress>> ret_t;
  ret_t rcounts;
  rcounts.reserve(counts.size());
  for(const auto& p : counts)
    rcounts.push_back({p.second, p.first});
  numentries = rcounts.size() < numentries ? rcounts.size() : numentries;
  partial_sort(rcounts.begin(), rcounts.begin()+numentries, rcounts.end(), [](const ret_t::value_type&a, const ret_t::value_type&b)
	       {
		 return(b.first < a.first);
	       });
  std::unordered_map<int, vector<boost::variant<string,double>>> ret;
  uint64_t rest = 0;
  unsigned int count = 1;
  for(const auto& rc : rcounts) {
    if(count==numentries+1) {
      rest+=rc.first;
    }
    else {
      ret.insert({count++, {rc.second.toString(), rc.first, 100.0*rc.first/total}});
    }
  }
  ret.insert({count, {"Rest", rest, total > 0 ? 100.0*rest/total : 100.0}});
  return ret;
}
