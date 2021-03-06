#include "BreakingNews.hpp"

const std::string hrp = "lat";

void BreakingNews::init()
{
    _mOwner.self() = std::pair<platon::Address, bool>(platon::platon_origin(), true);
    mNewsCount.self() = 0;
    mVPCount.self() = 0;
}

std::string BreakingNews::getOwner()
{
    return platon::encode(_mOwner.self().first, hrp);
}

void BreakingNews::setApprover(const std::string& approverAddress)
{
    // The caller might be the multisigner
    auto userAddress = platon::platon_caller();
    if (_mOwner.self().first != userAddress)
    {
        PLATON_EMIT_EVENT1(BNMessage, "setApprover" , "Unauthorized");
        return;
    }

    auto approverAddr = platon::make_address(approverAddress);
    if (approverAddr.second)
    {
        _mApprover.self() = approverAddr;
        PLATON_EMIT_EVENT1(BNMessage, "setApprover" , "success");
    }
    else{
        PLATON_EMIT_EVENT1(BNMessage, "setApprover" , "Invalid input!");
    }
}

std::string BreakingNews::createNews(const std::string& title,
                                  const std::string& content, 
                                  std::vector<std::string>& image, 
                                  const std::string& createTime)
{
    auto userAddress = platon::platon_origin();
    auto userAddrStr = platon::encode(userAddress, hrp);

    if (!_mApprover.self().second)
    {
        PLATON_EMIT_EVENT1(BNMessage, "createNews" , "Invalid Approver Address!");
        return "failed";
    }
    
    auto result = platon::platon_call(_mApprover.self().first, (unsigned int)(0), (unsigned int)(0), "createNewsDraft", 
                                                                    title, 
                                                                    content, 
                                                                    image,
                                                                    createTime, 
                                                                    userAddrStr);

    if (result)
    {
        PLATON_EMIT_EVENT1(BNMessage, "createNews" , "success!");
        return "success";
    }
    else{
        PLATON_EMIT_EVENT1(BNMessage, "createNews" , "Cross contract call failed!");
        return "failed";
    }
}

std::string BreakingNews::createViewPoint(platon::u128 ID,
                                const std::string& content,
                                std::vector<std::string>& image,
                                bool isSupported,
                                const std::string& createTime)
{
    auto userAddress = platon::platon_origin();
    auto userAddrStr = platon::encode(userAddress, hrp);

    if (!_mApprover.self().second)
    {
        PLATON_EMIT_EVENT1(BNMessage, "createViewPoint" , "Invalid Approver Address!");
        return "failed";
    }

    auto result = platon::platon_call(_mApprover.self().first, (unsigned int)(0), (unsigned int)(0), "createViewPointDraft", 
                                                                    ID, 
                                                                    content, 
                                                                    image, 
                                                                    isSupported, 
                                                                    createTime, 
                                                                    userAddrStr);

    if (result)
    {
        PLATON_EMIT_EVENT1(BNMessage, "createViewPoint" , "success!");
        return "success";
    }
    else{
        PLATON_EMIT_EVENT1(BNMessage, "createViewPoint" , "Cross contract call failed!");
        return "failed";
    }
}

void BreakingNews::approveNews(const std::string& title,
                                  const std::string& content, 
                                  std::vector<std::string>& image, 
                                  const std::string& createTime,
                                  const std::string& authorAddress)
{
    // judge caller
    auto callerAddress = platon::platon_caller();
    if (callerAddress != _mApprover.self().first)
    {
        PLATON_EMIT_EVENT1(BNMessage, "approveNews" , "Invalid approver!");
        return;
    }
    
    //insert news
    News curNews;
    curNews.NewTitle = title;
    curNews.NewID = mNewsCount.self();
    curNews.msgauthorAddress = authorAddress;
    curNews.msgContent = content;
    curNews.msgImages = image;
    curNews.BlockNumber = platon_block_number();
    curNews.createTime = createTime;
    curNews.Credibility = 0;

    //get user
    auto userPtr = _getUser(curNews.msgauthorAddress);
    //???????????????????????????????????????
    if (mUsers.cend() == userPtr)
    {
        PLATON_EMIT_EVENT1(AddNews, "Error: NULL when _getUser", curNews);
        return;
    }
    //??????????????????News??????????????????
    /***********************************/
    curNews.Cn_author = _mSysParams.self().News_gama * userPtr->UserCredibility / _mSysParams.self().Coefficient;
    curNews.Credibility = curNews.Cn_author;

    // userPtr->createNews_update(curNews.Credibility, this);
    mUsers.modify(userPtr, [&](auto& userItem){
        userItem.createNews_update(curNews.Credibility, this);
    });

    mBreakingNews[curNews.NewID] = curNews;
    PLATON_EMIT_EVENT1(AddNews, "Create News" , curNews);

    ++mNewsCount.self();
}

void BreakingNews::approveViewpoint(platon::u128 ID,
                                const std::string& content,
                                std::vector<std::string>& image,
                                bool isSupported,
                                const std::string& createTime,
                                const std::string& authorAddress)
{
    // judge caller
    auto callerAddress = platon::platon_caller();
    if (callerAddress != _mApprover.self().first)
    {
        PLATON_EMIT_EVENT1(BNMessage, "approveViewpoint" , "Invalid approver!");
        return;
    }

    //?????????news????????????
    bool isFound = false;

    if (mBreakingNews.contains(ID))
    {
        isFound = true;

        //insert viewpoint
        Viewpoint curVP;
        curVP.point = isSupported;
        curVP.ViewpointID = mVPCount.self();
        curVP.NewID = ID;
        curVP.msgauthorAddress = authorAddress;
        curVP.msgContent = content;
        curVP.msgImages = image;
        curVP.BlockNumber = platon_block_number();
        curVP.createTime = createTime;
        curVP.Credibility = 0;

        //get user
        auto userPtr = _getUser(curVP.msgauthorAddress);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "Create Viewpoint", "error: NULL when _getUser");
            return;
        }
        //??????????????????vp????????????Viewpoint??????News??????????????????
        /***********************************/
        auto newsItr = &(mBreakingNews[ID]);

        int32_t isSupport = curVP.point ? 1 : -1;
        curVP.Cv_author = isSupport * _mSysParams.self().View_alpha * newsItr->Credibility / _mSysParams.self().Coefficient;
        curVP.Cv_N = _mSysParams.self().View_gama * userPtr->UserCredibility / _mSysParams.self().Coefficient;
        curVP.Credibility = curVP.Cv_author + curVP.Cv_N;

        // userPtr->createView_update(curVP.Credibility, this);
        mUsers.modify(userPtr, [&](auto& userItem){
            userItem.createView_update(curVP.Credibility, this);
        });

        int32_t beforeCreNews_V = mBreakingNews[ID].Cn_V;
        newsItr->Cn_V = _mSysParams.self().rho * newsItr->Cn_V / _mSysParams.self().Coefficient +
            _mSysParams.self().News_alpha * curVP.Credibility * isSupport * (1 * _mSysParams.self().Coefficient - _mSysParams.self().rho) / _mSysParams.self().Coefficient;
        int32_t delta_Cn_V = newsItr->Cn_V - beforeCreNews_V;
        newsItr->Credibility += delta_Cn_V;
        newsItr->delta_Cn += delta_Cn_V;

        if ((newsItr->delta_Cn >= _mSysParams.self().News_threshold) ||
            (newsItr->delta_Cn <= -_mSysParams.self().News_threshold))
        {
            newsItr->updateNews(this);
        }

        mVP.emplace([&](auto& vpItem) {
		    vpItem = curVP;
		});

        ++mVPCount.self();
    }
    
    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "Create Viewpoint" , "error: news not found!");
        return;
    }

    PLATON_EMIT_EVENT1(BNMessage, "Create Viewpoint" , "success");
}

std::list<UserInfo> BreakingNews::getUsers()
{
    // return mUsers.self();
    std::list<UserInfo> user_info;

    for (auto userItr = mUsers.cbegin(); userItr != mUsers.cend(); ++userItr)
    {
        user_info.push_back(*userItr);
    }
    
    return user_info;
}

std::list<News> BreakingNews::getNews()
{
    std::list<News> News_Output;

    auto news_count = mNewsCount.self();
    for (auto i = 0; i < news_count; i++)
    {
        if (mBreakingNews.contains(i))
        {
            News curNews = mBreakingNews[i];

            auto normalIndexes = mVP.get_index<"NewsID"_n>();
            for (auto vpItr = normalIndexes.cbegin(curNews.NewID); vpItr != normalIndexes.cend(curNews.NewID); ++vpItr)
            {
                curNews.Viewpoints.push_back(*vpItr);
            }
            
            News_Output.push_back(curNews);
        }
    }
    
    return News_Output;
}

//???news?????????????????????????????????
//like???dislike?????????????????????????????????????????????????????????news???????????????
std::string BreakingNews::likeNews(platon::u128 newsID)
{
    auto userAddress = platon::platon_origin();
    std::string userAddrStr = platon::encode(userAddress, hrp);

    bool isFound = false;

    if (mBreakingNews.contains(newsID))
    {
        auto userPtr = _getUser(userAddrStr);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like news", "error: NULL when _getUser");
            return "error: NULL when _getUser";
        }
        isFound = true;

        auto newsItr = &(mBreakingNews[newsID]);

        //?????????disLike????????????
        newsItr->cancleDislike(&(*userPtr), this);
        
        //?????????like????????????????????????
        newsItr->addLike(&(*userPtr), this);

        //??????news???????????????????????????
        if ((newsItr->delta_Cn >= _mSysParams.self().News_threshold) ||
            (newsItr->delta_Cn <= -_mSysParams.self().News_threshold))
        {
            newsItr->updateNews(this);
        }
    }
    
    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "like news" , "error: news not found!");
        return "error: news not found!";
    }

    PLATON_EMIT_EVENT1(BNMessage, "like new" , "success");
    return "success";
}

std::string BreakingNews::cancellikeNews(platon::u128 newsID)
{
    auto userAddress = platon::platon_origin();
    std::string userAddrStr = platon::encode(userAddress, hrp);

    bool isFound = false;

    if (mBreakingNews.contains(newsID))
    {
        auto userPtr = _getUser(userAddrStr);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like news", "error: NULL when _getUser");
            return "error: NULL when _getUser";
        }
        isFound = true;

        auto newsItr = &(mBreakingNews[newsID]);
        //??????Like????????????
        newsItr->cancleLike(&(*userPtr), this);

        //??????news???????????????????????????
        if ((newsItr->delta_Cn >= _mSysParams.self().News_threshold) ||
            (newsItr->delta_Cn <= -_mSysParams.self().News_threshold))
        {
            newsItr->updateNews(this);
        }
    }

    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "cancel like news" , "error: news not found!");
        return "error: news not found!";
    }

    PLATON_EMIT_EVENT1(BNMessage, "cancel like news" , "success");
    return "success";
}

std::string BreakingNews::dislikeNews(platon::u128 newsID)
{
    auto userAddress = platon::platon_origin();
    std::string userAddrStr = platon::encode(userAddress, hrp);

    bool isFound = false;

    if (mBreakingNews.contains(newsID)){
        auto userPtr = _getUser(userAddrStr);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like news", "error: NULL when _getUser");
            return "error: NULL when _getUser";
        }

        isFound = true;

        auto newsItr = &(mBreakingNews[newsID]);
        //?????????Like????????????
        newsItr->cancleLike(&(*userPtr), this);
        
        //?????????disLike????????????????????????
        newsItr->addDislike(&(*userPtr), this);

        //??????news???????????????????????????
        if ((newsItr->delta_Cn >= _mSysParams.self().News_threshold) ||
            (newsItr->delta_Cn <= -_mSysParams.self().News_threshold))
        {
            newsItr->updateNews(this);
        }
    }

    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "dislike news" , "error: news not found!");
        return "error: news not found!";
    }

    PLATON_EMIT_EVENT1(BNMessage, "dislike news" , "success");
    return "success";
}

std::string BreakingNews::canceldislikeNews(platon::u128 newsID)
{
    auto userAddress = platon::platon_origin();
    std::string userAddrStr = platon::encode(userAddress, hrp);

    bool isFound = false;

    if (mBreakingNews.contains(newsID))
    {
        auto userPtr = _getUser(userAddrStr);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like news", "error: NULL when _getUser");
            return "error: NULL when _getUser";
        }

        isFound = true;
        
        auto newsItr = &(mBreakingNews[newsID]);
        //?????????disLike????????????
        newsItr->cancleDislike(&(*userPtr), this);

        //??????news???????????????????????????
        if ((newsItr->delta_Cn >= _mSysParams.self().News_threshold) ||
            (newsItr->delta_Cn <= -_mSysParams.self().News_threshold))
        {
            newsItr->updateNews(this);
        }
    }

    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "cancel dislike news" , "error: news not found!");
        return "error: news not found!";
    }

    PLATON_EMIT_EVENT1(BNMessage, "cancel dislike news" , "success");
    return "success";
}

//???Viewpoint?????????????????????????????????
//like???dislike?????????????????????????????????????????????????????????Viewpoint???????????????
std::string BreakingNews::likeViewpoint(platon::u128 vpID)
{
    auto userAddress = platon::platon_origin();
    std::string userAddrStr = platon::encode(userAddress, hrp);

    bool isFound = false;

    auto vpItr = mVP.find<"VPID"_n>(vpID);
    if (vpItr != mVP.cend())
    {
       auto userPtr = _getUser(userAddrStr);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like viewpoint", "error: NULL when _getUser");
            return "error: NULL when _getUser";
        }

        isFound = true;

        News* newsPtr = _getNews(vpItr->NewID);
        //???????????????????????????????????????
        if (NULL == newsPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like viewpoint", "error: NULL when _getNews");
            return "error: NULL when _getNews";
        }

        //?????????dislike????????????
        // vpItr->cancleDislike(userPtr, newsPtr, this);

        //?????????like????????????
        // vpItr->addLike(userPtr, newsPtr, this);

        mVP.modify(vpItr, [&](auto& vpItem){
            //?????????dislike????????????
            vpItem.cancleDislike(&(*userPtr), newsPtr, this);

            //?????????like????????????
            vpItem.addLike(&(*userPtr), newsPtr, this);
        });

        //????????Cv????????????????????????????????????user
        if ((vpItr->delta_Cv >= _mSysParams.self().View_threshold) || 
            (vpItr->delta_Cv <= -_mSysParams.self().View_threshold))
        {
            // vpItr->updateView(this);
            mVP.modify(vpItr, [&](auto& vpItem){
                vpItem.updateView(this);
            });
        }
    }

    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "like Viewpoint" , "error: Viewpoint not found!");
        return "error: news not found!";
    }

    PLATON_EMIT_EVENT1(BNMessage, "like Viewpoint" , "success");
    return "success";
}

std::string BreakingNews::cancellikeViewpoint(platon::u128 vpID)
{
    auto userAddress = platon::platon_origin();
    std::string userAddrStr = platon::encode(userAddress, hrp);

    bool isFound = false;

    auto vpItr = mVP.find<"VPID"_n>(vpID);
    if (vpItr != mVP.cend())
    {
        auto userPtr = _getUser(userAddrStr);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like news", "error: NULL when _getUser");
            return "error: NULL when _getUser";
        }

        isFound = true;

        News* newsPtr = _getNews(vpItr->NewID);
        //???????????????????????????????????????
        if (NULL == newsPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "cancel like viewpoint", "error: NULL when _getNews");
            return "error: NULL when _getNews";
        }

        //?????????like????????????
        // vpItr->cancleLike(userPtr, newsPtr, this);
        mVP.modify(vpItr, [&](auto& vpItem){
            //?????????like????????????
            vpItem.cancleLike(&(*userPtr), newsPtr, this);
        });

        //????????Cv????????????????????????????????????user
        if ((vpItr->delta_Cv >= _mSysParams.self().View_threshold) ||
            (vpItr->delta_Cv <= -_mSysParams.self().View_threshold))
        {
            // vpItr->updateView(this);
            mVP.modify(vpItr, [&](auto& vpItem){
                vpItem.updateView(this);
            });
        }
    }

    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "cancel like Viewpoint" , "error: Viewpoint not found!");
        return "error: news not found!";
    }

    PLATON_EMIT_EVENT1(BNMessage, "cancel like Viewpoint" , "success");
    return "success";
}

std::string BreakingNews::dislikeViewpoint(platon::u128 vpID)
{
    auto userAddress = platon::platon_origin();
    std::string userAddrStr = platon::encode(userAddress, hrp);

    bool isFound = false;

    auto vpItr = mVP.find<"VPID"_n>(vpID);
    if (vpItr != mVP.cend())
    {
        auto userPtr = _getUser(userAddrStr);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like news", "error: NULL when _getUser");
            return "error: NULL when _getUser";
        }

        isFound = true;

        News* newsPtr = _getNews(vpItr->NewID);
        //???????????????????????????????????????
        if (NULL == newsPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "dislike viewpoint", "error: NULL when _getNews");
            return "error: NULL when _getNews";
        }

        //?????????Like????????????
        // vpItr->cancleLike(userPtr, newsPtr, this);

        //?????????Dislike???
        // vpItr->addDislike(userPtr, newsPtr, this);

        mVP.modify(vpItr, [&](auto& vpItem){
            //?????????Like????????????
            vpItem.cancleLike(&(*userPtr), newsPtr, this);

            //?????????Dislike???
            vpItem.addDislike(&(*userPtr), newsPtr, this);
        });

        //????????Cv????????????????????????????????????user
        if ((vpItr->delta_Cv >= _mSysParams.self().View_threshold) ||
            (vpItr->delta_Cv <= -_mSysParams.self().View_threshold))
        {
            // vpItr->updateView(this);
            mVP.modify(vpItr, [&](auto& vpItem){
                vpItem.updateView(this);
            });
        }
    }

    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "dislike Viewpoint" , "error: Viewpoint not found!");
        return "error: news not found!";
    }

    PLATON_EMIT_EVENT1(BNMessage, "dislike Viewpoint" , "success");
    return "success";
}

std::string BreakingNews::canceldislikeViewpoint(platon::u128 vpID)
{
    auto userAddress = platon::platon_origin();
    std::string userAddrStr = platon::encode(userAddress, hrp);

    bool isFound = false;

    auto vpItr = mVP.find<"VPID"_n>(vpID);
    if (vpItr != mVP.cend())
    {
        auto userPtr = _getUser(userAddrStr);
        //???????????????????????????????????????
        if (mUsers.cend() == userPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "like news", "error: NULL when _getUser");
            return "error: NULL when _getUser";
        }

        isFound = true;

        News* newsPtr = _getNews(vpItr->NewID);
        //???????????????????????????????????????
        if (NULL == newsPtr)
        {
            PLATON_EMIT_EVENT1(BNMessage, "cancel dislike viewpoint", "error: NULL when _getNews");
            return "error: NULL when _getNews";
        }

        //?????????dislike????????????
        // vpItr->cancleDislike(userPtr, newsPtr, this);
        mVP.modify(vpItr, [&](auto& vpItem){
            //?????????like????????????
            vpItem.cancleDislike(&(*userPtr), newsPtr, this);
        });

        //????????Cv????????????????????????????????????user
        if ((vpItr->delta_Cv >= _mSysParams.self().View_threshold) ||
            (vpItr->delta_Cv <= -_mSysParams.self().View_threshold))
        {
            // vpItr->updateView(this);
            mVP.modify(vpItr, [&](auto& vpItem){
                vpItem.updateView(this);
            });
        }
    }

    if (!isFound)
    {
        PLATON_EMIT_EVENT1(BNMessage, "cancel dislike Viewpoint" , "error: Viewpoint not found!");
        return "error: news not found!";
    }

    PLATON_EMIT_EVENT1(BNMessage, "cancel dislike Viewpoint" , "success");
    return "success";
}

//????????????
bool BreakingNews::checkNews()
{
    std::list<News> News_Output = getNews();

    auto outPutNewsItr = News_Output.begin();
    if (News_Output.end() != outPutNewsItr)
    {
        PLATON_EMIT_EVENT1(AddNews, "Create News" , *outPutNewsItr);
    }
    else
    {
        PLATON_EMIT_EVENT1(AddNews, "Create News" , News());
    }

    return true;
}

//??????????????????
//??????
void BreakingNews::clear()
{
    auto userAddress = platon::platon_caller();
    if (_mOwner.self().first != userAddress)
    {
        return;
    }

    // clear news
    auto news_count = mNewsCount.self();
    for (auto i = 0; i < news_count; i++)
    {
        if (mBreakingNews.contains(i))
        {
            mBreakingNews.erase(i);
        }
    }
    mNewsCount.self() = 0;
    
    // clear users
    auto userItr = mUsers.cbegin();
    while (userItr != mUsers.cend())
    {
        auto tempItr = userItr;
        ++userItr;
        mUsers.erase(tempItr);
    }
    
    // clear viewpoints
    auto vpItr = mVP.cbegin();
    while (vpItr != mVP.cend())
    {
        auto tempItr = vpItr;
        ++vpItr;
        mVP.erase(tempItr);
    }
    mVPCount.self() = 0;
}

void BreakingNews::clearNews(platon::u128 newsID)
{
    auto userAddress = platon::platon_caller();
    if (_mOwner.self().first != userAddress)
    {
        return;
    }

    if (mBreakingNews.contains(newsID))
    {
        mBreakingNews.erase(newsID);
    }

    auto normalVPIndexs = mVP.get_index<"NewsID"_n>();
    auto vpItr = normalVPIndexs.cbegin(newsID);
    while (vpItr != normalVPIndexs.cend(newsID))
    {
        auto tmpItr = vpItr;
        ++vpItr;
        normalVPIndexs.erase(tmpItr);
    }
}

void BreakingNews::clearViewpoint(platon::u128 vpID)
{
    auto userAddress = platon::platon_caller();
    if (_mOwner.self().first != userAddress)
    {
        return;
    }

    auto vpItr = mVP.find<"VPID"_n>(vpID);
    if (vpItr != mVP.cend())
    {
        mVP.erase(vpItr);
    }
}

BreakingNews::usermi::const_iterator BreakingNews::_getUser(const std::string& userAddr)
{
    auto userItr = mUsers.find<"UserAddress"_n>(userAddr);
    if (userItr != mUsers.cend())
    {
        return userItr;
    }
    
    //??????????????????????????????
    UserInfo curUser;
    curUser.UserAddress = userAddr;
    curUser.UserCredibility = _mSysParams.self().User_init;
    
    // insert
    auto rst = mUsers.emplace([&](auto& userItem){
        userItem = curUser;
    });

    if (rst.second){
        return rst.first;
    }
    else{
        return mUsers.cend();
    }
}

//BreakingNews class add interface
News* BreakingNews::_getNews(const platon::u128& newsID)
{
    if (mBreakingNews.contains(newsID))
    {
        return &(mBreakingNews[newsID]);
    }
    
    return NULL;
}

auto BreakingNews::_getViewpoint(const platon::u128& vpID)
{
    return mVP.find<"VPID"_n>(vpID);
}

sysParams* BreakingNews::_getSysParams()
{
    return &(_mSysParams.self());
}

void BreakingNews::_emit_bnmessage_event(const std::string& topic, const std::string& msg)
{
    PLATON_EMIT_EVENT1(BNMessage, topic, msg);
}

//////////////////////////////////////////////////////////////////////////
//News
//??????????????????????????????news?????????
void News::addLike(const UserInfo* userPtr, BreakingNews* bnPtr)
{
	//?????????like????????????????????????
	auto sameItr = msgUp.begin();
	bool find = false;
	while (sameItr != msgUp.end())
	{
		if (*sameItr == userPtr->UserAddress)
		{
			find = true;
			break;
		}

		++sameItr;
	}

	if (!find)
	{
		msgUp.push_back(userPtr->UserAddress);

		//???????????????
		/*********************************/
        up_down_CreUpdate(userPtr, 1, bnPtr);
	}
}

void News::cancleLike(const UserInfo* userPtr, BreakingNews* bnPtr)
{
	//??????Like????????????
	auto LikeItr = msgUp.begin();
	while (LikeItr != msgUp.end())
	{
		if (*LikeItr == userPtr->UserAddress)
		{
			msgUp.erase(LikeItr);

			//???????????????
			/*********************************/
            up_down_CreUpdate(userPtr, -1, bnPtr);

			break;
		}

		++LikeItr;
	}
}

void News::addDislike(const UserInfo* userPtr, BreakingNews* bnPtr)
{
	//?????????disLike????????????????????????
	auto sameItr = msgDown.begin();
	bool find = false;
	while (sameItr != msgDown.end())
	{
		if (*sameItr == userPtr->UserAddress)
		{
			find = true;
			break;
		}

		++sameItr;
	}

	if (!find)
	{
		msgDown.push_back(userPtr->UserAddress);

        //???????????????
        /*********************************/
        up_down_CreUpdate(userPtr, -1, bnPtr);
	}
}

void News::cancleDislike(const UserInfo* userPtr, BreakingNews* bnPtr)
{
	auto dislikeItr = msgDown.begin();
	while (dislikeItr != msgDown.end())
	{
		if (*dislikeItr == userPtr->UserAddress)
		{
			msgDown.erase(dislikeItr);

            //???????????????
            /*********************************/
            up_down_CreUpdate(userPtr, 1, bnPtr);

			break;
		}

		++dislikeItr;
	}
}

void News::updataWithCv(int32_t delta_Cv, int32_t isSupport, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t delta_Cn_by_Cv = spPtr->News_alpha * isSupport * delta_Cv * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;
    Cn_V += delta_Cn_by_Cv;
    Credibility += delta_Cn_by_Cv;
    delta_Cn += delta_Cn_by_Cv;
}

void News::updateNews(BreakingNews* bnPtr)
{
    //??????view
    auto normal_indexs = bnPtr->mVP.get_index<"NewsID"_n>();
    for (auto vpItr = normal_indexs.cbegin(NewID); vpItr != normal_indexs.cend(NewID); ++vpItr)
    {
        // vpItr->delta_Cn_updata(delta_Cn, bnPtr);
        normal_indexs.modify(vpItr, [&](auto& vpItem){
            vpItem.delta_Cn_updata(delta_Cn, bnPtr);
        });
    }
    
    //??????user???????????????news???????????????user
    //author
    auto authorPtr = bnPtr->_getUser(msgauthorAddress);
    //???????????????????????????????????????
    if (bnPtr->mUsers.cend() == authorPtr)
    {
        bnPtr->_emit_bnmessage_event("updateNews", "error: NULL when _getUser");
        return;
    }
    // authorPtr->delta_News_updata_author(delta_Cn, bnPtr);
    bnPtr->mUsers.modify(authorPtr, [&](auto& authorItem){
        authorItem.delta_News_updata_author(delta_Cn, bnPtr);
    });

    //up users
    for (auto userAddrItr = msgUp.begin(); userAddrItr != msgUp.end(); ++userAddrItr)
    {
        auto userPtr = bnPtr->_getUser(*userAddrItr);
        //???????????????????????????????????????
        if (bnPtr->mUsers.cend() == userPtr)
        {
            bnPtr->_emit_bnmessage_event("updateNews", "error: NULL when _getUser");
            return;
        }
        // userPtr->delta_News_update_up_down(delta_Cn, 1, bnPtr);
        bnPtr->mUsers.modify(userPtr, [&](auto& userItem){
            userItem.delta_News_update_up_down(delta_Cn, 1, bnPtr);
        });
    }

    //down users
    for (auto userAddrItr = msgDown.begin(); userAddrItr != msgDown.end(); ++userAddrItr)
    {
        auto userPtr = bnPtr->_getUser(*userAddrItr);
        //???????????????????????????????????????
        if (bnPtr->mUsers.cend() == userPtr)
        {
            bnPtr->_emit_bnmessage_event("updateNews", "error: NULL when _getUser");
            return;
        }
        // userPtr->delta_News_update_up_down(delta_Cn, -1, bnPtr);
        bnPtr->mUsers.modify(userPtr, [&](auto& userItem){
            userItem.delta_News_update_up_down(delta_Cn, -1, bnPtr);
        });
    }

    delta_Cn = 0;
}

void News::up_down_CreUpdate(const UserInfo* userPtr, int32_t coe, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t before_Cn_up_down = Cn_up_down;

    Cn_up_down = Cn_up_down * spPtr->rho / spPtr->Coefficient + 
        coe * spPtr->News_beta * userPtr->UserCredibility * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;

    int32_t delta_Cn_up_down = Cn_up_down - before_Cn_up_down;
    Credibility += delta_Cn_up_down;
    delta_Cn += delta_Cn_up_down;
}


//////////////////////////////////////////////////////////////////////////
//Viewpoints
//??????????????????????????????VP?????????
void Viewpoint::addLike(const UserInfo* userPtr, News* newsPtr, BreakingNews* bnPtr)
{
	//?????????like????????????????????????
	auto sameItr = msgUp.begin();
	bool find = false;
	while (sameItr != msgUp.end())
	{
		if (*sameItr == userPtr->UserAddress)
		{
			find = true;
			break;
		}

		++sameItr;
	}

	if (!find)
	{
		msgUp.push_back(userPtr->UserAddress);

		//???????????????
		/*********************************/
        up_down_CreUpdate(userPtr, newsPtr, 1, bnPtr);
	}
}

void Viewpoint::cancleLike(const UserInfo* userPtr, News* newsPtr, BreakingNews* bnPtr)
{
	//??????Like????????????
	auto LikeItr = msgUp.begin();
	while (LikeItr != msgUp.end())
	{
		if (*LikeItr == userPtr->UserAddress)
		{
			msgUp.erase(LikeItr);

			//???????????????
			/*********************************/
            up_down_CreUpdate(userPtr, newsPtr, -1, bnPtr);

			break;
		}

		++LikeItr;
	}
}

void Viewpoint::addDislike(const UserInfo* userPtr, News* newsPtr, BreakingNews* bnPtr)
{
	//?????????disLike????????????????????????
	auto sameItr = msgDown.begin();
	bool find = false;
	while (sameItr != msgDown.end())
	{
		if (*sameItr == userPtr->UserAddress)
		{
			find = true;
			break;
		}

		++sameItr;
	}

	if (!find)
	{
		msgDown.push_back(userPtr->UserAddress);

        //???????????????
        /*********************************/
        up_down_CreUpdate(userPtr, newsPtr, -1, bnPtr);
	}
}

void Viewpoint::cancleDislike(const UserInfo* userPtr, News* newsPtr, BreakingNews* bnPtr)
{
	auto dislikeItr = msgDown.begin();
	while (dislikeItr != msgDown.end())
	{
		if (*dislikeItr == userPtr->UserAddress)
		{
			msgDown.erase(dislikeItr);
			//???????????????
			/*********************************/
            up_down_CreUpdate(userPtr, newsPtr, 1, bnPtr);

			break;
		}

		++dislikeItr;
	}
}

void Viewpoint::up_down_CreUpdate(const UserInfo* userPtr, News* newsPtr, int32_t coe, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t before_Cv_up_down = Cv_up_down;
    Cv_up_down = Cv_up_down * spPtr->rho / spPtr->Coefficient +
        spPtr->View_beta * userPtr->UserCredibility * coe * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;

    int32_t delta_Cv_up_down = Cv_up_down - before_Cv_up_down;
    Credibility += delta_Cv_up_down;
    delta_Cv += delta_Cv_up_down;

    //???????????????news
    int32_t coe_isSupport = point ? 1 : -1;
    newsPtr->updataWithCv(delta_Cv_up_down, coe_isSupport, bnPtr);
}

void Viewpoint::updateView(BreakingNews* bnPtr)
{
    //update related users
    //author
    auto authorPtr = bnPtr->_getUser(msgauthorAddress);
    //???????????????????????????????????????
    if (bnPtr->mUsers.cend() == authorPtr)
    {
        bnPtr->_emit_bnmessage_event("updateView", "error: NULL when _getUser");
        return;
    }
    // authorPtr->delta_View_updata_author(delta_Cv, bnPtr);
    bnPtr->mUsers.modify(authorPtr, [&](auto& authorItem){
        authorItem.delta_View_updata_author(delta_Cv, bnPtr);
    });

    //up
    for (auto userAddrItr = msgUp.begin(); userAddrItr != msgUp.end(); ++userAddrItr)
    {
        auto userPtr = bnPtr->_getUser(*userAddrItr);
        //???????????????????????????????????????
        if (bnPtr->mUsers.cend() == userPtr)
        {
            bnPtr->_emit_bnmessage_event("updateView", "error: NULL when _getUser");
            return;
        }
        // userPtr->delta_View_update_up_down(delta_Cv, 1, bnPtr);
        bnPtr->mUsers.modify(userPtr, [&](auto& userItem){
            userItem.delta_View_update_up_down(delta_Cv, 1, bnPtr);
        });
    }

    //down
    for (auto userAddrItr = msgDown.begin(); userAddrItr != msgDown.end(); ++userAddrItr)
    {
        auto userPtr = bnPtr->_getUser(*userAddrItr);
        //???????????????????????????????????????
        if (bnPtr->mUsers.cend() == userPtr)
        {
            bnPtr->_emit_bnmessage_event("updateView", "error: NULL when _getUser");
            return;
        }
        // userPtr->delta_View_update_up_down(delta_Cv, -1, bnPtr);
        bnPtr->mUsers.modify(userPtr, [&](auto& userItem){
            userItem.delta_View_update_up_down(delta_Cv, -1, bnPtr);
        });
    }

    delta_Cv = 0;
}

void Viewpoint::delta_Cn_updata(int32_t delta_Cn, BreakingNews* bnPtr)
{
    int32_t before_Cv_N = Cv_N;

    int32_t coe = point ? 1 : -1;
    sysParams* spPtr = bnPtr->_getSysParams();

    Cv_N += coe * delta_Cn * spPtr->View_alpha / spPtr->Coefficient;
    int32_t delta_Cv_N = Cv_N - before_Cv_N;

    Credibility += delta_Cv_N;
    delta_Cv += delta_Cv_N;
}

////////////////////////////////////////////////////////////////////////////////////
//UserInfo methods
void UserInfo::CredibilityAdjust(BreakingNews* bnPtr)
{
    if (UserCredibility <= 0)
    {
        //?????????output???????????????????????????????????????1??????
        UserCredibility = 1;
    }
}

void UserInfo::createNews_update(int32_t C_News, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t before_Cu_N_author = Cu_N_author;
    Cu_N_author = Cu_N_author * spPtr->rho / spPtr->Coefficient +
        spPtr->User_alpha * C_News * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;

    int32_t delta_Cu_Nauthor = Cu_N_author - before_Cu_N_author;
    UserCredibility += delta_Cu_Nauthor;

    CredibilityAdjust(bnPtr);
}

void UserInfo::createView_update(int32_t C_View, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t before_Cu_V_author = Cu_V_author;
    Cu_V_author = Cu_V_author * spPtr->rho / spPtr->Coefficient +
        spPtr->User_beta * C_View * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;

    int32_t delta_Cu_Vauthor = Cu_V_author - before_Cu_V_author;
    UserCredibility += delta_Cu_Vauthor;

    CredibilityAdjust(bnPtr);
}

void UserInfo::delta_News_updata_author(int32_t delta_Cn, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t before_Cu_N_author = Cu_N_author;
    Cu_N_author += spPtr->User_alpha * delta_Cn * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;
    int32_t delta_Cu_Nauthor = Cu_N_author - before_Cu_N_author;

    UserCredibility += delta_Cu_Nauthor;

    CredibilityAdjust(bnPtr);
}

void UserInfo::delta_News_update_up_down(int32_t delta_Cn, int32_t isUp, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t before_Cu_N_up_down = Cu_N_up_down;
    Cu_N_up_down += spPtr->User_gama * isUp * delta_Cn * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;

    int32_t delta_Cu_Nupdown = Cu_N_up_down - before_Cu_N_up_down;

    UserCredibility += delta_Cu_Nupdown;

    CredibilityAdjust(bnPtr);
}

void UserInfo::delta_View_updata_author(int32_t delta_Cv, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t before_Cu_V_author = Cu_V_author;
    Cu_V_author += spPtr->User_beta * delta_Cv * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;
    int32_t delta_Cu_Vauthor = Cu_V_author - before_Cu_V_author;

    UserCredibility += delta_Cu_Vauthor;

    CredibilityAdjust(bnPtr);
}

void UserInfo::delta_View_update_up_down(int32_t delta_Cv, int32_t isUp, BreakingNews* bnPtr)
{
    sysParams* spPtr = bnPtr->_getSysParams();

    int32_t before_Cu_V_up_down = Cu_V_up_down;
    Cu_V_up_down += spPtr->User_eta * isUp * delta_Cv * (1 * spPtr->Coefficient - spPtr->rho) / spPtr->Coefficient;

    int32_t delta_Cu_Vupdown = Cu_V_up_down - before_Cu_V_up_down;

    UserCredibility += delta_Cu_Vupdown;

    CredibilityAdjust(bnPtr);
}
