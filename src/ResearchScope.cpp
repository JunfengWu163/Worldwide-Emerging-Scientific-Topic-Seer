#include "ResearchScope.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <StringProcessing.h>
#include <sqlite3.h>
#include <time.h>
#include <CallbackData.h>
#include <wxFFileLog.h>

std::vector<std::string> ResearchScope::getResearchScopes(const std::string path)
{
    std::vector<std::string> results;
    sqlite3 *db = NULL;
    int rc = sqlite3_open(path.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        logError(wxT("Cannot open database at" + path));
        return results;
    }

    const char *sql = "SELECT keywords FROM researchScopes ORDERY BY update_time ASC;";
    CallbackData data;
    char *errorMessage = NULL;
    rc = sqlite3_exec(db, sql, sqliteCallback, &data, &errorMessage);
    if (rc == SQLITE_OK)
    {
        for (auto &result: data.results)
        {
            results.push_back(result["keywords"]);
        }
    }
    else
    {
        logError(errorMessage);
    }
    sqlite3_close(db);
    return results;
}

ResearchScope::ResearchScope(const std::string path, const std::string kws1, const std::string kws2)
{
    //ctor
    _path = path;
    _kws1 = splitString(normalize(kws1), ",");
    _kws2 = splitString(normalize(kws2), ",");
    std::sort(_kws1.begin(), _kws1.end());
    std::sort(_kws2.begin(), _kws2.end());
}

ResearchScope::ResearchScope(const std::string path, const std::string keywords)
{
    //ctor
    _path = path;
    std::vector<std::string> kws = splitString(keywords,";");
    if (kws.size() != 2)
        throw std::invalid_argument("invalid keywords");
    _kws1 = splitString(kws[0], ",");
    _kws2 = splitString(kws[1], ",");
    std::sort(_kws1.begin(), _kws1.end());
    std::sort(_kws2.begin(), _kws2.end());
}

ResearchScope::~ResearchScope()
{
    //dtor
}

bool ResearchScope::storable()
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open(_path.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        logError(wxT("Cannot open database at" + _path));
        return false;
    }
    char *errorMessage = NULL;
    const char*sqls[] =
    {
        "CREATE TABLE IF NOT EXISTS publications("
        "id INTEGER PRIMARY KEY ASC,"
        "year INTEGER,"
        "title TEXT,"
        "abstract TEXT,"
        "source TEXT,"
        "language TEXT,"
        "authors TEXT,"
        "ref_ids TEXT);",

        "CREATE TABLE IF NOT EXISTS openalex_queries("
        "combination TEXT,"
        "year INTEGER,"
        "update_time INTEGER,"
        "ids TEXT,"
        "ref_ids TEXT,"
        "PRIMARY KEY(combination,year));",

        "CREATE TABLE IF NOT EXISTS openalex_tokens("
        "combination TEXT,"
        "year INTEGER,"
        "update_time INTEGER,"
        "PRIMARY KEY(combination,year));",

        "CREATE TABLE IF NOT EXISTS research_scopes("
        "keywords TEXT PRIMARY KEY,"
        "combinations TEXT,"
        "update_time INTEGER);",

        "CREATE TABLE IF NOT EXISTS pub_terms("
        "id INTEGER PRIMARY KEY ASC,"
        "terms TEXT);",

        "CREATE TABLE IF NOT EXISTS pub_scope_terms("
        "id INTEGER,"
        "scope_keywords TEXT,"
        "year INTEGER,"
        "update_time INTEGER,"
        "terms TEXT,"
        "PRIMARY KEY(id,scope_keywords))",

        "CREATE TABLE IF NOT EXISTS scope_terms("
        "keywords TEXT,"
        "year INTEGER,"
        "update_time INTEGER,"
        "terms TEXT,"
        "PRIMARY KEY(keywords,year));",
    };
    for (const char*sql: sqls)
    {
        rc = sqlite3_exec(db, sql, NULL, NULL, &errorMessage);
        if (rc != SQLITE_OK)
        {
            logError(errorMessage);
            sqlite3_close(db);
            return false;
        }
    }

    sqlite3_close(db);
    return true;
}

std::string ResearchScope::getKeywords()
{
    std::stringstream ss;
    for (size_t i = 0; i < _kws1.size(); i++)
    {
        if (i > 0)
        {
            ss << ",";
        }
        ss << _kws1[i];
    }
    ss << ";";
    for (size_t i = 0; i < _kws2.size(); i++)
    {
        if (i > 0)
        {
            ss << ",";
        }
        ss << _kws2[i];
    }
    return ss.str();
}

std::string ResearchScope::getCombinations()
{
    std::stringstream ss;
    for (size_t i1 = 0; i1 < _kws1.size(); i1++)
    {
        std::string s1 = _kws1[i1];
        for (size_t i2 = 0; i2 < _kws2.size(); i2++)
        {
            if (i1 > 0 || i2 > 0)
                ss << ",";
            std::string s2 = _kws2[i2];
            if (s1 < s2)
                ss << s1 << "&" << s2;
            else
                ss << s2 << "&" << s1;
        }
    }
    return ss.str();
}

int ResearchScope::numCombinations() const
{
    return _kws1.size() * _kws2.size();
}

std::string ResearchScope::getCombination(int i) const
{
    int temp = i % numCombinations();
    int i1 = temp % _kws1.size();
    temp /= _kws1.size();
    int i2 = temp;
    if (_kws1[i1] < _kws2[i2])
        return _kws1[i1] + "&" + _kws2[i2];
    else
        return _kws2[i2] + "&" + _kws1[i1];
}

bool ResearchScope::init()
{
    if (!storable())
        return false;

    sqlite3 *db = NULL;
    int rc = sqlite3_open(_path.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        logError(wxT("Cannot open database at" + _path));
        return false;
    }
    CallbackData data;
    char *errorMessage = NULL;

    std::string keywords = getKeywords();
    std::string combinations = getCombinations();
    time_t t;
    time(&t);
    std::stringstream ss;
    ss << "INSERT OR IGNORE INTO research_scopes(keywords,combinations,update_time) VALUES('"
       << keywords << "','" << combinations << "'," << (int)t
       << ");";
    std::string sql = ss.str();
    rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errorMessage);
    sqlite3_close(db);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
    }
    return rc == SQLITE_OK;
}

bool ResearchScope::load(int idxComb, const int y, std::map<uint64_t, Publication> &pubsOfY)
{
    pubsOfY.clear();
    sqlite3 *db = NULL;
    int rc = sqlite3_open(_path.c_str(), &db);
    if (rc != SQLITE_OK)
        return false;
    CallbackData data;
    char *errorMessage = NULL;

    std::stringstream ss;
    ss << "SELECT combination, year, ids FROM openalex_queries"
       " WHERE combination = '" << getCombination(idxComb) << "' AND year = " << y << ";";
    rc = sqlite3_exec(db, ss.str().c_str(), sqliteCallback, &data, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
    }
    if (rc != SQLITE_OK || data.results.size() == 0)
    {
        sqlite3_close(db);
        return false;
    }

    std::string strIds = data.results[0]["ids"];
    data.results.clear();
    ss.clear();
    ss << "SELECT id, year, title, abstract, source, language, authors, ref_ids FROM publications WHERE id in ("
       << strIds << ");";
    rc = sqlite3_exec(db, ss.str().c_str(), sqliteCallback, &data, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
        sqlite3_close(db);
        return false;
    }

    for (auto &result: data.results)
    {
        Publication pub(result);
        pubsOfY[pub.id()] = pub;
    }
    sqlite3_close(db);
    return true;
}

bool ResearchScope::load(int idxComb, const int y)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open(_path.c_str(), &db);
    if (rc != SQLITE_OK)
        return false;
    CallbackData data;
    char *errorMessage = NULL;

    std::stringstream ss;
    ss << "SELECT combination, year FROM openalex_tokens"
       " WHERE combination = '" << getCombination(idxComb) << "' AND year = " << y << ";";
    rc = sqlite3_exec(db, ss.str().c_str(), sqliteCallback, &data, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
    }
    if (rc != SQLITE_OK || data.results.size() == 0)
    {
        sqlite3_close(db);
        return false;
    }
    sqlite3_close(db);
    return true;
}

bool ResearchScope::save(const std::map<uint64_t, Publication> &pubs)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open(_path.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        logError(wxT("Cannot open database at" + _path));
        return false;
    }
    CallbackData data;
    char *errorMessage = NULL;

    // ---------------------------------------------------------------------------
    // step 1: find old ids
    std::stringstream ss;
    ss << "SELECT id FROM publications where id in (";
    int iPub = 0;
    for (auto idToPub: pubs)
    {
        if (iPub++ > 0)
            ss << ",";
        ss << idToPub.first;
    }
    ss << ");";
    rc = sqlite3_exec(db, ss.str().c_str(), sqliteCallback, &data, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
        sqlite3_close(db);
        return false;
    }

    if (data.results.size() == pubs.size())
    {
        sqlite3_close(db);
        return true;
    }

    std::set<uint64_t> oldIdSet;
    for (auto result: data.results)
    {
        oldIdSet.insert(std::stoull(result["id"]));
    }

    // ---------------------------------------------------------------------------
    // step 2: insert publications with new ids
    ss.clear();
    ss << "INSERT INTO publications(id, year, title, abstract, source, language, authors, ref_ids) VALUES ";
    iPub = 0;
    for (auto idToPub: pubs)
    {
        if (oldIdSet.find(idToPub.first) != oldIdSet.end())
            continue;
        if (iPub++ > 0)
            ss << ",";
        ss << "("
           << idToPub.second.id() << ","
           << idToPub.second.year() << ",'"
           << idToPub.second.title() << "','"
           << idToPub.second.abstract() << "','"
           << idToPub.second.source() << "','"
           << idToPub.second.language() << "','";
        std::vector<std::string> authors = idToPub.second.authors();
        for (size_t i = 0; i < authors.size(); i++)
        {
            if (i > 0)
                ss << ",";
            ss << authors[i];
        }
        ss << "','";
        std::vector<uint64_t> refIds = idToPub.second.refIds();
        for (size_t i = 0; i < refIds.size(); i++)
        {
            if (i > 0)
                ss << ",";
            ss << refIds[i];
        }
        ss << "')";
    }
    ss << ";";

    rc = sqlite3_exec(db, ss.str().c_str(), NULL, NULL, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
    }
    sqlite3_close(db);
    return rc == SQLITE_OK;
}

bool ResearchScope::save(int idxComb, const int y, const std::map<uint64_t, Publication> &pubsOfY)
{
    save(pubsOfY);

    sqlite3 *db = NULL;
    int rc = sqlite3_open(_path.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        logError(wxT("Cannot open database at" + _path));
        return false;
    }
    CallbackData data;
    char *errorMessage = NULL;

    // ---------------------------------------------------------------------------
    // step 1: get ref ids
    std::set<uint64_t> refIds;
    for (auto idToPub: pubsOfY)
    {
        vector<uint64_t> refIdsOfPub = idToPub.second.refIds();
        refIds.insert(refIdsOfPub.begin(), refIdsOfPub.end());
    }

    // ---------------------------------------------------------------------------
    // step 2: save ids and ref ids
    std::string combination = getCombination(idxComb);
    time_t t;
    time(&t);
    std::stringstream ss;
    ss << "INSERT INTO openalex_queries(combination,year,update_time,ids,ref_ids) VALUES ('"
       << combination << "'," << y << "," << (int) t << ",'";
    int iPub = 0;
    for (auto idToPub: pubsOfY)
    {
        if (iPub++ > 0)
            ss << ",";
        ss << idToPub.first;
    }
    ss << "','";
    int iRef = 0;
    for (uint64_t refId: refIds)
    {
        if (iRef++ > 0)
            ss << ",";
        ss << refId;
    }
    ss << "');";
    rc = sqlite3_exec(db, ss.str().c_str(), NULL, NULL, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
    }
    sqlite3_close(db);
    return rc == SQLITE_OK;
}

bool ResearchScope::save(int idxComb, const int y)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open(_path.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        logError(wxT("Cannot open database at" + _path));
        return false;
    }
    CallbackData data;
    char *errorMessage = NULL;

    std::string combination = getCombination(idxComb);
    time_t t;
    time(&t);
    std::stringstream ss;
    ss << "INSERT INTO openalex_tokens(combination,year,update_time) VALUES ('"
       << combination << "'," << y << "," << (int) t << ");";
    rc = sqlite3_exec(db, ss.str().c_str(), NULL, NULL, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
    }
    sqlite3_close(db);
    return rc == SQLITE_OK;
}

bool ResearchScope::getMissingRefIds(int idxComb, const int y, std::vector<uint64_t> &newRefIds)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open(_path.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        logError(wxT("Cannot open database at" + _path));
        return false;
    }
    CallbackData data;
    char *errorMessage = NULL;

    // ---------------------------------------------------------------------------
    // step 1: get ref ids of combination and year
    std::stringstream ss;
    ss << "SELECT combination, year, ref_ids FROM openalex_queries"
       " WHERE combination = '" << getCombination(idxComb) << "' AND year = " << y << ";";
    rc = sqlite3_exec(db, ss.str().c_str(), sqliteCallback, &data, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
    }
    if (rc != SQLITE_OK || data.results.size() == 0)
    {
        sqlite3_close(db);
        return false;
    }
    std::string refIdsStr = data.results[0]["ref_ids"];
    std::vector<std::string> refIDStrs = splitString(refIdsStr, ",");
    std::vector<uint64_t> refIds;
    for (std::string s : refIDStrs)
    {
        refIds.push_back(std::stoull(s));
    }

    // ---------------------------------------------------------------------------
    // step 2: find old ids
    ss.clear();
    data.results.clear();
    ss << "SELECT id FROM publications WHERE id IN (" << refIdsStr << ");";
    rc = sqlite3_exec(db, ss.str().c_str(), sqliteCallback, &data, &errorMessage);
    if (rc != SQLITE_OK)
    {
        logError(errorMessage);
    }
    if (rc != SQLITE_OK || data.results.size() == 0)
    {
        sqlite3_close(db);
        return false;
    }
    std::set<uint64_t> oldRefIds;
    for (auto result: data.results)
    {
        oldRefIds.insert(std::stoull(result["id"]));
    }

    // ---------------------------------------------------------------------------
    // step 3: output new ids
    newRefIds.clear();
    for (uint64_t id: refIds)
    {
        if (oldRefIds.find(id) == oldRefIds.end())
        {
            newRefIds.push_back(id);
        }
    }
    return true;
}
