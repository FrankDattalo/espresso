#pragma once

namespace espresso {

class Runtime;

namespace bytecode {

void Load(Runtime* rt);

void Verify(Runtime* rt);

} // bytecode

} // espresso