package xlog

import "fmt"

const (
	LOG_LEVEL_DEBUG = iota
	LOG_LEVEL_INFO
	LOG_LEVEL_WARN
	LOG_LEVEL_ERROR
	LOG_LEVEL_FATAL
	LOG_LEVEL_OFF
)

type Logger struct {
	level int16
}

func NewLogger() *Logger {
	return &Logger{
		level: LOG_LEVEL_OFF,
	}
}

func (l *Logger) SetLevel(level int16) {
	l.level = level
}

func (l *Logger) Printf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
}

func (l *Logger) Debugf(format string, args ...interface{}) {
	if l.level > LOG_LEVEL_DEBUG {
		return
	}
	l.Printf(format, args...)
}

func (l *Logger) Infof(format string, args ...interface{}) {
	if l.level > LOG_LEVEL_INFO {
		return
	}
	l.Printf(format, args...)
}

func (l *Logger) Warnf(format string, args ...interface{}) {
	if l.level > LOG_LEVEL_WARN {
		return
	}
	l.Printf(format, args...)
}

func (l *Logger) Errorf(format string, args ...interface{}) {
	if l.level > LOG_LEVEL_ERROR {
		return
	}
	l.Printf(format, args...)
}

func (l *Logger) Fatalf(format string, args ...interface{}) {
	if l.level > LOG_LEVEL_FATAL {
		return
	}
	l.Printf(format, args...)
}

func (l *Logger) Panicf(format string, args ...interface{}) {
	l.Printf(format, args...)
	panic(fmt.Sprintf(format, args...))
}
