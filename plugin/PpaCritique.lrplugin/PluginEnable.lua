local LrLogger = import 'LrLogger'

local ServiceLifecycle = require 'ServiceLifecycle'

local logger = LrLogger('TheJury')
logger:enable('logfile')
logger:info('The Jury plugin enabled')
ServiceLifecycle.start_managed_service_async()
