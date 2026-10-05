using Android.App;
using Android.Runtime;

namespace BmwE9x.EdiabasBridge.Android;

[Application]
public sealed class MainApplication : Application
{
    public MainApplication(IntPtr handle, JniHandleOwnership ownership) : base(handle, ownership) { }
}
