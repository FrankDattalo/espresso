#pragma once

namespace espresso {

class Runtime;

namespace native {

void RegisterNatives(Runtime* rt);

void Entrypoint(Runtime* rt);

}

}