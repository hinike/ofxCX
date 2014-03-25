Framebuffers and Buffer Swapping {#framebufferSwapping}
================================

Somes pieces of terminology that come up a lot in the documentation for CX are framebuffer, front buffer, back buffer, and swap buffers. What exactly are these things?

A framebuffer is fairly easy to explain in the rough by example. The contents of the screen of a computer are stored in a framebuffer. A framebuffer is essentially a rectangle of pixels where each pixel can be set to display any color. Framebuffers do not always have the same number of pixels as the screen: you can have framebuffers that are smaller or larger than the size of the screen. Framebuffers larger than the screen don't really do much for you as you cannot fit the whole thing on the screen. In CX, framebuffers are typically worked with through the abstraction of an ofFbo.

There are two special framebuffers: The front buffer and the back buffer. These are created by OpenGL automatically as part of starting OpenGL. The size of these special framebuffers is functionally the same as the size of the window (or the whole screen, if in full screen mode). The front buffer contains what is shown on the screen. The back buffer is not presented on the screen, so it can be rendered to at any time without affecting what is visible on the screen. Typically, when you render stuff in CX, you call CX::CX_Display::beginDrawingToBackBuffer() and CX::CX_Display::endDrawingToBackBuffer() around whatever you are rendering. This causes drawing that happens between the two function calls to be rendered to the back buffer. 

What you have rendered to the back buffer has no effect on what you see on screen until you swap the contents of the front and back buffers. This isn't a true swap, in that that the back buffer does not end up with the contents of the front buffer in it. In reality, the back buffer is copied to the front buffer and is itself unchanged. This swap can be done by using different functions of the CX_Display: BLOCKING_swapFrontAndBackBuffers(), swapFrontAndBackBuffers(), or BLOCKING_setAutoSwapping(). These functions are not interchangable, so make sure you are using the right one for your application.