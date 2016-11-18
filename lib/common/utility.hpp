#pragma once

namespace hydra
{

#define BEFORE_MAIN_IMPL(block, __line__)       \
    namespace                                   \
    {                                           \
        class __beforeMainHelper__##__line__    \
        {                                       \
        public:                                 \
            __beforeMainHelper__##__line__()    \
            {                                   \
                block;                          \
            }                                   \
        };                                      \
                                                \
        static __beforeMainHelper__##__line__   \
        __beforeMainHelper_instant__##__line__; \
    }

#define BEFORE_MAIN(block)  BEFORE_MAIN_IMPL(block, __LINE__) 

}