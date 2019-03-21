#include <logger.hh>
#include <async_logger.hh>

#include <string>

namespace eve
{

static async_logger alogger;

void init_log_file(const std::string &file)
{
    alogger.set_log_file(file);
}

logger::logger()
{
}

logger::~logger()
{
    alogger.append(ss.str()+"\n");
}

} // namespace eve
