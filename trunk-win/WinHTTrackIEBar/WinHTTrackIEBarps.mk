
WinHTTrackIEBarps.dll: dlldata.obj WinHTTrackIEBar_p.obj WinHTTrackIEBar_i.obj
	link /dll /out:WinHTTrackIEBarps.dll /def:WinHTTrackIEBarps.def /entry:DllMain dlldata.obj WinHTTrackIEBar_p.obj WinHTTrackIEBar_i.obj \
		kernel32.lib rpcndr.lib rpcns4.lib rpcrt4.lib oleaut32.lib uuid.lib \

.c.obj:
	cl /c /Ox /DWIN32 /D_WIN32_WINNT=0x0400 /DREGISTER_PROXY_DLL \
		$<

clean:
	@del WinHTTrackIEBarps.dll
	@del WinHTTrackIEBarps.lib
	@del WinHTTrackIEBarps.exp
	@del dlldata.obj
	@del WinHTTrackIEBar_p.obj
	@del WinHTTrackIEBar_i.obj
