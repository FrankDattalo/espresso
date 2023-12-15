#pragma once

namespace espresso {

class Runtime;

namespace native {

void RegisterNatives(Runtime* rt);

namespace debugger {

void Breakpoint(Runtime* runtime);

} // debugger

}

}