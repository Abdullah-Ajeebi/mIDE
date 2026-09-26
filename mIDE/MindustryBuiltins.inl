#include <string>
#include <vector>

// Auto-generated built-in registrar
template<typename T>
void RegisterMindustryBuiltins(T& functions_) {
    functions_.insert({ L"ubind", { { L"type" }, true, false } });
    functions_.insert({ L"ulocate", { { L"locate", L"flag", L"enemy", L"ore", L"outY", L"outFound", L"outBuild" }, true, true } });
    functions_.insert({ L"ucontrol", { { L"type", L"p1", L"p2", L"p3", L"p4", L"p5" }, true, false } });
    functions_.insert({ L"control", { { L"type", L"target", L"p1", L"p2", L"p3", L"p4" }, true, false } });
    functions_.insert({ L"getlink", { { L"index" }, true, true } });
    functions_.insert({ L"read", { { L"target", L"position" }, true, true } });
    functions_.insert({ L"write", { { L"target", L"position", L"value" }, true, false } });
    functions_.insert({ L"sensor", { { L"from", L"type" }, true, true } });
    functions_.insert({ L"radar", { { L"target1", L"target2", L"target3", L"sort", L"radar", L"sortOrder" }, true, true } });
    functions_.insert({ L"select", { { L"op", L"comp0", L"comp1", L"a", L"b" }, true, true } });
    functions_.insert({ L"end", { {  }, true, false } });
    functions_.insert({ L"draw", { { L"type", L"x", L"y", L"p1", L"p2", L"p3", L"p4" }, true, false } });
    functions_.insert({ L"drawflush", { { L"target" }, true, false } });
    functions_.insert({ L"print", { { L"value" }, true, false } });
    functions_.insert({ L"printchar", { { L"value" }, true, false } });
    functions_.insert({ L"format", { { L"value" }, true, false } });
    functions_.insert({ L"printflush", { { L"target" }, true, false } });
    functions_.insert({ L"setrate", { { L"amount" }, true, false } });
    functions_.insert({ L"wait", { { L"value" }, true, false } });
    functions_.insert({ L"stop", { {  }, true, false } });
    functions_.insert({ L"lookup", { { L"from", L"type" }, true, true } });
    functions_.insert({ L"packcolor", { { L"r", L"g", L"b", L"a" }, true, true } });
    functions_.insert({ L"unpackcolor", { { L"r", L"g", L"b", L"a", L"value" }, true, false } });
    functions_.insert({ L"cutscene", { { L"action", L"p1", L"p2", L"p3", L"p4" }, true, false } });
    functions_.insert({ L"fetch", { { L"type", L"team", L"extra", L"index" }, true, true } });
    functions_.insert({ L"query", { { L"shape", L"type", L"team", L"x", L"y", L"width", L"height" }, true, false } });
    functions_.insert({ L"getblock", { { L"x", L"y", L"layer" }, true, true } });
    functions_.insert({ L"setblock", { { L"x", L"y", L"block", L"team", L"rotation", L"layer" }, true, false } });
    functions_.insert({ L"spawnunit", { { L"type", L"x", L"y", L"rotation", L"team", L"effect" }, true, true } });
    functions_.insert({ L"spawnbullet", { { L"from", L"index", L"x", L"y", L"rotation", L"team", L"owner", L"damage", L"velocityScl", L"lifeScl", L"aimX", L"aimY" }, true, true } });
    functions_.insert({ L"setweather", { { L"type", L"state" }, true, false } });
    functions_.insert({ L"applyeffect", { { L"clear", L"effect", L"unit", L"duration" }, true, false } });
    functions_.insert({ L"setrule", { { L"rule", L"value", L"p1", L"p2", L"p3", L"p4" }, true, false } });
    functions_.insert({ L"flushmessage", { { L"type", L"duration", L"outSuccess" }, true, false } });
    functions_.insert({ L"effect", { { L"type", L"x", L"y", L"rotation", L"color", L"data" }, true, false } });
    functions_.insert({ L"explosion", { { L"team", L"x", L"y", L"radius", L"damage", L"air", L"ground", L"pierce", L"effect" }, true, false } });
    functions_.insert({ L"sync", { { L"variable" }, true, false } });
    functions_.insert({ L"clientdata", { { L"channel", L"value", L"reliable" }, true, false } });
    functions_.insert({ L"getflag", { { L"flag" }, true, true } });
    functions_.insert({ L"setflag", { { L"flag", L"value" }, true, false } });
    functions_.insert({ L"spawnwave", { {  }, true, false } });
    functions_.insert({ L"setprop", { { L"type", L"of", L"value" }, true, false } });
    functions_.insert({ L"playsound", { {  }, true, false } });
    functions_.insert({ L"playmusic", { {  }, true, false } });
    functions_.insert({ L"setmarker", { { L"type", L"id", L"p1", L"p2", L"p3" }, true, false } });
    functions_.insert({ L"makemarker", { { L"type", L"id", L"x", L"y", L"replace" }, true, false } });
    functions_.insert({ L"localeprint", { { L"name" }, true, false } });
}


// Auto-generated instruction emitter
template<typename TEmit, typename TNewTemp>
bool TryEmitMindustryBuiltin(const std::wstring& name, const std::vector<std::wstring>& arguments, TEmit&& Emit, TNewTemp&& NewTemporary, std::wstring& outResult) {
    if (name == L"ubind") {
        Emit(L"ubind " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"ulocate") {
        std::wstring temp = NewTemporary();
        Emit(L"ulocate " + temp + L" " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5] + L" " + arguments[6]);
        outResult = temp;
        return true;
    }
    if (name == L"ucontrol") {
        Emit(L"ucontrol " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5]);
        outResult = L"0";
        return true;
    }
    if (name == L"control") {
        Emit(L"control " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5]);
        outResult = L"0";
        return true;
    }
    if (name == L"getlink") {
        std::wstring temp = NewTemporary();
        Emit(L"getlink " + temp + L" " + arguments[0]);
        outResult = temp;
        return true;
    }
    if (name == L"read") {
        std::wstring temp = NewTemporary();
        Emit(L"read " + temp + L" " + arguments[0] + L" " + arguments[1]);
        outResult = temp;
        return true;
    }
    if (name == L"write") {
        Emit(L"write " + arguments[0] + L" " + arguments[1] + L" " + arguments[2]);
        outResult = L"0";
        return true;
    }
    if (name == L"sensor") {
        std::wstring temp = NewTemporary();
        Emit(L"sensor " + temp + L" " + arguments[0] + L" " + arguments[1]);
        outResult = temp;
        return true;
    }
    if (name == L"radar") {
        std::wstring temp = NewTemporary();
        Emit(L"radar " + temp + L" " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5]);
        outResult = temp;
        return true;
    }
    if (name == L"select") {
        std::wstring temp = NewTemporary();
        Emit(L"select " + temp + L" " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4]);
        outResult = temp;
        return true;
    }
    if (name == L"end") {
        Emit(L"end");
        outResult = L"0";
        return true;
    }
    if (name == L"draw") {
        Emit(L"draw " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5] + L" " + arguments[6]);
        outResult = L"0";
        return true;
    }
    if (name == L"drawflush") {
        Emit(L"drawflush " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"print") {
        Emit(L"print " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"printchar") {
        Emit(L"printchar " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"format") {
        Emit(L"format " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"printflush") {
        Emit(L"printflush " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"setrate") {
        Emit(L"setrate " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"wait") {
        Emit(L"wait " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"stop") {
        Emit(L"stop");
        outResult = L"0";
        return true;
    }
    if (name == L"lookup") {
        std::wstring temp = NewTemporary();
        Emit(L"lookup " + temp + L" " + arguments[0] + L" " + arguments[1]);
        outResult = temp;
        return true;
    }
    if (name == L"packcolor") {
        std::wstring temp = NewTemporary();
        Emit(L"packcolor " + temp + L" " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3]);
        outResult = temp;
        return true;
    }
    if (name == L"unpackcolor") {
        Emit(L"unpackcolor " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4]);
        outResult = L"0";
        return true;
    }
    if (name == L"cutscene") {
        Emit(L"cutscene " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4]);
        outResult = L"0";
        return true;
    }
    if (name == L"fetch") {
        std::wstring temp = NewTemporary();
        Emit(L"fetch " + temp + L" " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3]);
        outResult = temp;
        return true;
    }
    if (name == L"query") {
        Emit(L"query " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5] + L" " + arguments[6]);
        outResult = L"0";
        return true;
    }
    if (name == L"getblock") {
        std::wstring temp = NewTemporary();
        Emit(L"getblock " + temp + L" " + arguments[0] + L" " + arguments[1] + L" " + arguments[2]);
        outResult = temp;
        return true;
    }
    if (name == L"setblock") {
        Emit(L"setblock " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5]);
        outResult = L"0";
        return true;
    }
    if (name == L"spawnunit") {
        std::wstring temp = NewTemporary();
        Emit(L"spawnunit " + temp + L" " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5]);
        outResult = temp;
        return true;
    }
    if (name == L"spawnbullet") {
        std::wstring temp = NewTemporary();
        Emit(L"spawnbullet " + temp + L" " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5] + L" " + arguments[6] + L" " + arguments[7] + L" " + arguments[8] + L" " + arguments[9] + L" " + arguments[10] + L" " + arguments[11]);
        outResult = temp;
        return true;
    }
    if (name == L"setweather") {
        Emit(L"setweather " + arguments[0] + L" " + arguments[1]);
        outResult = L"0";
        return true;
    }
    if (name == L"applyeffect") {
        Emit(L"applyeffect " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3]);
        outResult = L"0";
        return true;
    }
    if (name == L"setrule") {
        Emit(L"setrule " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5]);
        outResult = L"0";
        return true;
    }
    if (name == L"flushmessage") {
        Emit(L"flushmessage " + arguments[0] + L" " + arguments[1] + L" " + arguments[2]);
        outResult = L"0";
        return true;
    }
    if (name == L"effect") {
        Emit(L"effect " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5]);
        outResult = L"0";
        return true;
    }
    if (name == L"explosion") {
        Emit(L"explosion " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4] + L" " + arguments[5] + L" " + arguments[6] + L" " + arguments[7] + L" " + arguments[8]);
        outResult = L"0";
        return true;
    }
    if (name == L"sync") {
        Emit(L"sync " + arguments[0]);
        outResult = L"0";
        return true;
    }
    if (name == L"clientdata") {
        Emit(L"clientdata " + arguments[0] + L" " + arguments[1] + L" " + arguments[2]);
        outResult = L"0";
        return true;
    }
    if (name == L"getflag") {
        std::wstring temp = NewTemporary();
        Emit(L"getflag " + temp + L" " + arguments[0]);
        outResult = temp;
        return true;
    }
    if (name == L"setflag") {
        Emit(L"setflag " + arguments[0] + L" " + arguments[1]);
        outResult = L"0";
        return true;
    }
    if (name == L"spawnwave") {
        Emit(L"spawnwave");
        outResult = L"0";
        return true;
    }
    if (name == L"setprop") {
        Emit(L"setprop " + arguments[0] + L" " + arguments[1] + L" " + arguments[2]);
        outResult = L"0";
        return true;
    }
    if (name == L"playsound") {
        Emit(L"playsound");
        outResult = L"0";
        return true;
    }
    if (name == L"playmusic") {
        Emit(L"playmusic");
        outResult = L"0";
        return true;
    }
    if (name == L"setmarker") {
        Emit(L"setmarker " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4]);
        outResult = L"0";
        return true;
    }
    if (name == L"makemarker") {
        Emit(L"makemarker " + arguments[0] + L" " + arguments[1] + L" " + arguments[2] + L" " + arguments[3] + L" " + arguments[4]);
        outResult = L"0";
        return true;
    }
    if (name == L"localeprint") {
        Emit(L"localeprint " + arguments[0]);
        outResult = L"0";
        return true;
    }
    return false;
}