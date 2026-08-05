#pragma once
#ifndef GROOVEPUTER_CARDPUTER_SMF_ROUTE_PERSISTENCE_H
#define GROOVEPUTER_CARDPUTER_SMF_ROUTE_PERSISTENCE_H

namespace GroovePuterPlatform {

#ifdef ARDUINO
void serviceCardputerSmfRoutePersistence();
#else
inline void serviceCardputerSmfRoutePersistence() {}
#endif

}  // namespace GroovePuterPlatform

#endif  // GROOVEPUTER_CARDPUTER_SMF_ROUTE_PERSISTENCE_H
