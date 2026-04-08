#ifndef SRC_HPP
#define SRC_HPP

#include <cstddef>

/**
 * 枚举类，用于枚举可能的置换策略
 */
enum class ReplacementPolicy { kDEFAULT = 0, kFIFO, kLRU, kMRU, kLRU_K };

/**
 * @brief 该类用于维护每一个页对应的信息以及其访问历史，用于在尝试置换时查询需要的信息。
 */
class PageNode {
public:
  PageNode(std::size_t id, std::size_t k) : page_id_(id), k_(k) {
    if (k_ > 0) {
      access_times_ = new std::size_t[k_];
    }
  }
  
  ~PageNode() {
    if (access_times_) {
      delete[] access_times_;
    }
  }
  
  void AddAccess(std::size_t time) {
    if (access_count_ == 0) {
      first_access_time_ = time;
    }
    last_access_time_ = time;
    if (k_ > 0) {
      access_times_[access_count_ % k_] = time;
    }
    access_count_++;
  }
  
  std::size_t GetFirstAccess() const { return first_access_time_; }
  std::size_t GetLastAccess() const { return last_access_time_; }
  std::size_t GetKthLastAccess() const {
    if (access_count_ < k_ || k_ == 0) return 0;
    return access_times_[access_count_ % k_];
  }
  std::size_t GetAddTime() const { return add_time_; }
  void SetAddTime(std::size_t time) { add_time_ = time; }
  std::size_t GetPageId() const { return page_id_; }
  std::size_t GetAccessCount() const { return access_count_; }

private:
  std::size_t page_id_{};
  std::size_t k_{};
  std::size_t access_count_{0};
  std::size_t* access_times_{nullptr};
  std::size_t add_time_{0};
  std::size_t first_access_time_{0};
  std::size_t last_access_time_{0};
};

class ReplacementManager {
public:
  constexpr static std::size_t npos = -1;

  ReplacementManager() = delete;

  ReplacementManager(std::size_t max_size, std::size_t k, ReplacementPolicy default_policy) 
      : max_size_(max_size), k_(k), default_policy_(default_policy), size_(0), time_step_(0) {
    pages_ = new PageNode*[max_size_];
  }

  ~ReplacementManager() {
    for (std::size_t i = 0; i < size_; ++i) {
      delete pages_[i];
    }
    delete[] pages_;
  }

  void SwitchDefaultPolicy(ReplacementPolicy default_policy) {
    default_policy_ = default_policy;
  }

  void Visit(std::size_t page_id, std::size_t &evict_id, ReplacementPolicy policy = ReplacementPolicy::kDEFAULT) {
    time_step_++;
    if (policy == ReplacementPolicy::kDEFAULT) {
      policy = default_policy_;
    }

    for (std::size_t i = 0; i < size_; ++i) {
      if (pages_[i]->GetPageId() == page_id) {
        pages_[i]->AddAccess(time_step_);
        evict_id = npos;
        return;
      }
    }

    if (size_ == max_size_) {
      std::size_t evict_idx = GetEvictIndex(policy);
      if (evict_idx != npos) {
        evict_id = pages_[evict_idx]->GetPageId();
        delete pages_[evict_idx];
        
        PageNode* new_node = new PageNode(page_id, k_);
        new_node->SetAddTime(time_step_);
        new_node->AddAccess(time_step_);
        pages_[evict_idx] = new_node;
      } else {
        evict_id = npos;
      }
    } else {
      evict_id = npos;
      PageNode* new_node = new PageNode(page_id, k_);
      new_node->SetAddTime(time_step_);
      new_node->AddAccess(time_step_);
      pages_[size_++] = new_node;
    }
  }

  bool RemovePage(std::size_t page_id) {
    for (std::size_t i = 0; i < size_; ++i) {
      if (pages_[i]->GetPageId() == page_id) {
        delete pages_[i];
        pages_[i] = pages_[size_ - 1];
        size_--;
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] std::size_t TryEvict(ReplacementPolicy policy = ReplacementPolicy::kDEFAULT) const {
    if (size_ < max_size_) return npos;
    std::size_t evict_idx = GetEvictIndex(policy);
    if (evict_idx != npos) {
      return pages_[evict_idx]->GetPageId();
    }
    return npos;
  }

  [[nodiscard]] bool Empty() const {
    return size_ == 0;
  }

  [[nodiscard]] bool Full() const {
    return size_ == max_size_;
  }

  [[nodiscard]] std::size_t Size() const {
    return size_;
  }

private:
  std::size_t GetEvictIndex(ReplacementPolicy policy) const {
    if (size_ == 0) return npos;
    if (policy == ReplacementPolicy::kDEFAULT) policy = default_policy_;
    
    std::size_t best_idx = 0;
    if (policy == ReplacementPolicy::kFIFO) {
      for (std::size_t i = 1; i < size_; ++i) {
        if (pages_[i]->GetAddTime() < pages_[best_idx]->GetAddTime()) {
          best_idx = i;
        }
      }
    } else if (policy == ReplacementPolicy::kLRU) {
      for (std::size_t i = 1; i < size_; ++i) {
        if (pages_[i]->GetLastAccess() < pages_[best_idx]->GetLastAccess()) {
          best_idx = i;
        }
      }
    } else if (policy == ReplacementPolicy::kMRU) {
      for (std::size_t i = 1; i < size_; ++i) {
        if (pages_[i]->GetLastAccess() > pages_[best_idx]->GetLastAccess()) {
          best_idx = i;
        }
      }
    } else if (policy == ReplacementPolicy::kLRU_K) {
      for (std::size_t i = 1; i < size_; ++i) {
        auto& curr = pages_[i];
        auto& best = pages_[best_idx];
        
        bool curr_inf = curr->GetAccessCount() < k_;
        bool best_inf = best->GetAccessCount() < k_;
        
        if (curr_inf && !best_inf) {
          best_idx = i;
        } else if (curr_inf && best_inf) {
          if (curr->GetFirstAccess() < best->GetFirstAccess()) {
            best_idx = i;
          }
        } else if (!curr_inf && !best_inf) {
          if (curr->GetKthLastAccess() < best->GetKthLastAccess()) {
            best_idx = i;
          }
        }
      }
    }
    return best_idx;
  }

  std::size_t max_size_;
  std::size_t k_;
  ReplacementPolicy default_policy_;
  std::size_t size_;
  std::size_t time_step_;
  PageNode** pages_;
};
#endif
