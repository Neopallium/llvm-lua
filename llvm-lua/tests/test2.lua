function f(x,y,z)
        print("hook",x,y,z)
end

function g()
        print"0"
        debug.sethook(f,"l")
        print"1"
        print"2"
        print"3"
        print"4"
end

print("pcall",pcall(g))

