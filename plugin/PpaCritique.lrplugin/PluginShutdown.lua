local LrLogger = import 'LrLogger'

local ServiceLifecycle = require 'ServiceLifecycle'

local logger = LrLogger('TheJury')
logger:enable('logfile')
logger:info('The Jury plugin shutting down')
ServiceLifecycle.stop_managed_service_now()
