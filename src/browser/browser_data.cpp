/**
 * browser_data.cpp
 * 
 * Browser data management implementation
 */

#include "browser_data.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <windows.h>

BrowserData::BrowserData(const std::string& dataDir) : m_dataDir(dataDir) {
    // Create data directory if it doesn't exist
    CreateDirectoryA(dataDir.c_str(), NULL);
    
    Load();
}

BrowserData::~BrowserData() {
    Save();
}

std::string BrowserData::GetHistoryFile() {
    return m_dataDir + "\\history.txt";
}

std::string BrowserData::GetBookmarksFile() {
    return m_dataDir + "\\bookmarks.txt";
}

std::string BrowserData::GetCacheFile() {
    return m_dataDir + "\\cache.dat";
}

/**
 * Add to history
 */
void BrowserData::AddHistory(const std::string& url, const std::string& title) {
    // Check if URL already exists
    auto it = std::find_if(m_history.begin(), m_history.end(),
                           [&url](const HistoryEntry& entry) {
                               return entry.url == url;
                           });
    
    if (it != m_history.end()) {
        // Update existing entry
        it->visitCount++;
        it->timestamp = time(nullptr);
        it->title = title;
    } else {
        // Add new entry
        HistoryEntry entry;
        entry.url = url;
        entry.title = title;
        entry.timestamp = time(nullptr);
        entry.visitCount = 1;
        
        m_history.insert(m_history.begin(), entry);
        
        // Limit history size
        if (m_history.size() > 1000) {
            m_history.pop_back();
        }
    }
    
    SaveHistory();
}

/**
 * Get history
 */
std::vector<HistoryEntry> BrowserData::GetHistory(int limit) {
    if (limit <= 0 || limit > (int)m_history.size()) {
        return m_history;
    }
    
    return std::vector<HistoryEntry>(m_history.begin(), m_history.begin() + limit);
}

/**
 * Clear history
 */
void BrowserData::ClearHistory() {
    m_history.clear();
    SaveHistory();
}

/**
 * Add bookmark
 */
void BrowserData::AddBookmark(const std::string& url, const std::string& title) {
    // Check if already bookmarked
    if (IsBookmarked(url)) {
        return;
    }
    
    Bookmark bookmark;
    bookmark.url = url;
    bookmark.title = title;
    bookmark.created = time(nullptr);
    
    m_bookmarks.push_back(bookmark);
    SaveBookmarks();
}

/**
 * Remove bookmark
 */
void BrowserData::RemoveBookmark(const std::string& url) {
    m_bookmarks.erase(
        std::remove_if(m_bookmarks.begin(), m_bookmarks.end(),
                       [&url](const Bookmark& bookmark) {
                           return bookmark.url == url;
                       }),
        m_bookmarks.end()
    );
    
    SaveBookmarks();
}

/**
 * Get bookmarks
 */
std::vector<Bookmark> BrowserData::GetBookmarks() {
    return m_bookmarks;
}

/**
 * Check if URL is bookmarked
 */
bool BrowserData::IsBookmarked(const std::string& url) {
    return std::find_if(m_bookmarks.begin(), m_bookmarks.end(),
                        [&url](const Bookmark& bookmark) {
                            return bookmark.url == url;
                        }) != m_bookmarks.end();
}

/**
 * Cache content
 */
void BrowserData::CacheContent(const std::string& url, const std::string& content) {
    CacheEntry entry;
    entry.url = url;
    entry.content = content;
    entry.timestamp = time(nullptr);
    entry.size = content.size();
    
    m_cache[url] = entry;
    
    // Limit cache size to 50MB
    size_t totalSize = GetCacheSize();
    if (totalSize > 50 * 1024 * 1024) {
        // Remove oldest entries
        std::vector<std::string> urls;
        for (const auto& pair : m_cache) {
            urls.push_back(pair.first);
        }
        
        std::sort(urls.begin(), urls.end(),
                  [this](const std::string& a, const std::string& b) {
                      return m_cache[a].timestamp < m_cache[b].timestamp;
                  });
        
        // Remove oldest 25%
        size_t toRemove = urls.size() / 4;
        for (size_t i = 0; i < toRemove; i++) {
            m_cache.erase(urls[i]);
        }
    }
    
    SaveCache();
}

/**
 * Get cached content
 */
bool BrowserData::GetCachedContent(const std::string& url, std::string& content) {
    auto it = m_cache.find(url);
    if (it != m_cache.end()) {
        // Check if cache is still valid (24 hours)
        time_t now = time(nullptr);
        if (now - it->second.timestamp < 24 * 3600) {
            content = it->second.content;
            return true;
        } else {
            // Remove expired cache
            m_cache.erase(it);
        }
    }
    
    return false;
}

/**
 * Clear cache
 */
void BrowserData::ClearCache() {
    m_cache.clear();
    SaveCache();
}

/**
 * Get total cache size
 */
size_t BrowserData::GetCacheSize() {
    size_t totalSize = 0;
    for (const auto& pair : m_cache) {
        totalSize += pair.second.size;
    }
    return totalSize;
}

/**
 * Load all data
 */
bool BrowserData::Load() {
    LoadHistory();
    LoadBookmarks();
    LoadCache();
    return true;
}

/**
 * Save all data
 */
bool BrowserData::Save() {
    SaveHistory();
    SaveBookmarks();
    SaveCache();
    return true;
}

/**
 * Load history from file
 */
bool BrowserData::LoadHistory() {
    std::ifstream file(GetHistoryFile());
    if (!file.is_open()) {
        return false;
    }
    
    m_history.clear();
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        HistoryEntry entry;
        
        // Format: timestamp|visitCount|url|title
        std::string timestampStr, visitCountStr;
        std::getline(iss, timestampStr, '|');
        std::getline(iss, visitCountStr, '|');
        std::getline(iss, entry.url, '|');
        std::getline(iss, entry.title);
        
        entry.timestamp = std::stoll(timestampStr);
        entry.visitCount = std::stoi(visitCountStr);
        
        m_history.push_back(entry);
    }
    
    file.close();
    return true;
}

/**
 * Load bookmarks from file
 */
bool BrowserData::LoadBookmarks() {
    std::ifstream file(GetBookmarksFile());
    if (!file.is_open()) {
        return false;
    }
    
    m_bookmarks.clear();
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        Bookmark bookmark;
        
        // Format: timestamp|url|title
        std::string timestampStr;
        std::getline(iss, timestampStr, '|');
        std::getline(iss, bookmark.url, '|');
        std::getline(iss, bookmark.title);
        
        bookmark.created = std::stoll(timestampStr);
        
        m_bookmarks.push_back(bookmark);
    }
    
    file.close();
    return true;
}

/**
 * Load cache from file
 */
bool BrowserData::LoadCache() {
    std::ifstream file(GetCacheFile(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    m_cache.clear();
    
    while (file.good()) {
        // Read URL length
        uint32_t urlLen = 0;
        file.read((char*)&urlLen, sizeof(urlLen));
        if (urlLen == 0 || urlLen > 10000) break;
        
        // Read URL
        std::string url(urlLen, '\0');
        file.read(&url[0], urlLen);
        
        // Read timestamp
        time_t timestamp;
        file.read((char*)&timestamp, sizeof(timestamp));
        
        // Read content length
        uint32_t contentLen = 0;
        file.read((char*)&contentLen, sizeof(contentLen));
        if (contentLen == 0 || contentLen > 10 * 1024 * 1024) break;
        
        // Read content
        std::string content(contentLen, '\0');
        file.read(&content[0], contentLen);
        
        CacheEntry entry;
        entry.url = url;
        entry.content = content;
        entry.timestamp = timestamp;
        entry.size = contentLen;
        
        m_cache[url] = entry;
    }
    
    file.close();
    return true;
}

/**
 * Save history to file
 */
bool BrowserData::SaveHistory() {
    std::ofstream file(GetHistoryFile());
    if (!file.is_open()) {
        return false;
    }
    
    for (const auto& entry : m_history) {
        file << entry.timestamp << "|"
             << entry.visitCount << "|"
             << entry.url << "|"
             << entry.title << "\n";
    }
    
    file.close();
    return true;
}

/**
 * Save bookmarks to file
 */
bool BrowserData::SaveBookmarks() {
    std::ofstream file(GetBookmarksFile());
    if (!file.is_open()) {
        return false;
    }
    
    for (const auto& bookmark : m_bookmarks) {
        file << bookmark.created << "|"
             << bookmark.url << "|"
             << bookmark.title << "\n";
    }
    
    file.close();
    return true;
}

/**
 * Save cache to file
 */
bool BrowserData::SaveCache() {
    std::ofstream file(GetCacheFile(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    for (const auto& pair : m_cache) {
        const CacheEntry& entry = pair.second;
        
        // Write URL length and URL
        uint32_t urlLen = (uint32_t)entry.url.length();
        file.write((char*)&urlLen, sizeof(urlLen));
        file.write(entry.url.c_str(), urlLen);
        
        // Write timestamp
        file.write((char*)&entry.timestamp, sizeof(entry.timestamp));
        
        // Write content length and content
        uint32_t contentLen = (uint32_t)entry.content.length();
        file.write((char*)&contentLen, sizeof(contentLen));
        file.write(entry.content.c_str(), contentLen);
    }
    
    file.close();
    return true;
}
