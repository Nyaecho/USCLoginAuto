class PingException(Exception):
    """Custom exception for ping-related errors."""
    pass

class getTokenAndCookieException(Exception):
    """Custom exception for token and cookie retrieval errors."""
    pass

class getStatusException(Exception):
    """Custom exception for status retrieval errors."""
    pass

class LogoutException(Exception):
    """Custom exception for login timeout errors."""
    pass

class ErrorException(Exception):
    """Custom exception for generic errors."""
    pass

class LoginException(Exception):
    """Custom exception for login-related errors."""
    pass